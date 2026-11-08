#pragma once

#include <charconv>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

#include "data.hpp"
#include "data_math.hpp"
#include "encode.hpp"
#include "error.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {

/**
 * A value in its JSON form: simply the text.
 *
 * forstio's equivalent sat on a ring buffer, because its codecs feed off
 * streaming I/O. Nothing in lbm's core streams JSON -- a config is read whole
 * and a document is written whole -- so the text itself is the representation
 * and the buffer machinery is not needed.
 */
template <typename Schema> class data<Schema, enc::Json> final {
public:
	using SchemaType = Schema;

private:
	std::string value_;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	explicit data(std::string value__) : value_{std::move(value__)} {}
	explicit data(std::string_view value__) : value_{value__} {}

	void set(std::string value__) { value_ = std::move(value__); }

	const std::string &stl_string() const { return value_; }

	std::string_view stl_view() const { return value_; }

	std::string &stl_string() { return value_; }

	data<sch::UInt64, enc::Native> size() const { return {value_.size()}; }
};

template <typename Encoding> struct codec_config;

template <> struct codec_config<enc::Json> {
	/**
	 * Indent nested values across multiple lines.
	 */
	bool pretty = false;
	/**
	 * Strict decoding demands a fully described dataset: every member of a
	 * Struct has to be present. Non strict is the default, since config files
	 * usually only state the deviations from the defaults.
	 */
	bool strict = false;
	/**
	 * Guards against pathological nesting in untrusted input.
	 */
	uint64_t depth = 64u;
};

namespace err {
struct json_malformed {
	static constexpr std::string_view description = "Malformed JSON";
	static constexpr bool is_critical = false;
};

struct json_type_mismatch {
	static constexpr std::string_view description =
		"JSON value does not match the schema";
	static constexpr bool is_critical = false;
};

struct json_too_deep {
	static constexpr std::string_view description = "JSON nesting is too deep";
	static constexpr bool is_critical = false;
};

struct json_missing_member {
	static constexpr std::string_view description =
		"JSON object is missing a required member";
	static constexpr bool is_critical = false;
};
} // namespace err

namespace impl {

/**
 * Cursor over the JSON text. Every read either advances past a well formed
 * token or leaves the position untouched and reports an error.
 */
class json_parser final {
private:
	std::string_view src_;
	uint64_t pos_;
	uint64_t depth_;
	uint64_t max_depth_;

public:
	json_parser(std::string_view src__, uint64_t max_depth__)
		: src_{src__}, pos_{0u}, depth_{0u}, max_depth_{max_depth__} {}

	uint64_t offset() const { return pos_; }

	bool at_end() const { return pos_ >= src_.size(); }

	void skip_whitespace() {
		while (pos_ < src_.size()) {
			const char c = src_[pos_];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				++pos_;
			} else {
				break;
			}
		}
	}

	error_or<char> peek() {
		skip_whitespace();
		if (at_end()) {
			return make_error<err::json_malformed>("Unexpected end of input");
		}
		return src_[pos_];
	}

	error_or<void> expect(char c) {
		auto eo_c = peek();
		if (eo_c.is_error()) {
			return std::move(eo_c.get_error());
		}
		if (eo_c.get_value() != c) {
			return make_error<err::json_malformed>("Unexpected character");
		}
		++pos_;
		return make_void();
	}

	bool consume_if(char c) {
		skip_whitespace();
		if (!at_end() && src_[pos_] == c) {
			++pos_;
			return true;
		}
		return false;
	}

	error_or<void> enter() {
		++depth_;
		if (depth_ > max_depth_) {
			return make_error<err::json_too_deep>();
		}
		return make_void();
	}

	void leave() {
		if (depth_ > 0u) {
			--depth_;
		}
	}

	/**
	 * Reads a quoted string, resolving the escapes JSON defines. \u is decoded
	 * to UTF-8; a lone surrogate is passed through as the replacement
	 * character rather than rejected, so one bad escape cannot sink a config.
	 */
	error_or<std::string> parse_string() {
		auto eo_q = expect('"');
		if (eo_q.is_error()) {
			return std::move(eo_q.get_error());
		}

		std::string out;
		while (true) {
			if (at_end()) {
				return make_error<err::json_malformed>("Unterminated string");
			}
			const char c = src_[pos_++];
			if (c == '"') {
				return out;
			}
			if (c != '\\') {
				out.push_back(c);
				continue;
			}
			if (at_end()) {
				return make_error<err::json_malformed>("Dangling escape");
			}
			const char e = src_[pos_++];
			switch (e) {
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			case 'u': {
				auto eo_cp = parse_hex4();
				if (eo_cp.is_error()) {
					return std::move(eo_cp.get_error());
				}
				uint32_t cp = eo_cp.get_value();
				if (cp >= 0xD800u && cp <= 0xDBFFu) {
					// High surrogate: a low one must follow to form a pair.
					if (pos_ + 1u < src_.size() && src_[pos_] == '\\' &&
						src_[pos_ + 1u] == 'u') {
						pos_ += 2u;
						auto eo_lo = parse_hex4();
						if (eo_lo.is_error()) {
							return std::move(eo_lo.get_error());
						}
						const uint32_t lo = eo_lo.get_value();
						if (lo >= 0xDC00u && lo <= 0xDFFFu) {
							cp = 0x10000u + ((cp - 0xD800u) << 10) +
								 (lo - 0xDC00u);
						} else {
							cp = 0xFFFDu;
						}
					} else {
						cp = 0xFFFDu;
					}
				} else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
					cp = 0xFFFDu;
				}
				append_utf8(out, cp);
				break;
			}
			default:
				return make_error<err::json_malformed>("Unknown escape");
			}
		}
	}

	/**
	 * Returns the raw token of a JSON number, leaving the numeric conversion
	 * to the caller who knows the target type.
	 */
	error_or<std::string_view> parse_number_token() {
		skip_whitespace();
		const uint64_t start = pos_;

		if (!at_end() && (src_[pos_] == '-' || src_[pos_] == '+')) {
			++pos_;
		}
		while (!at_end() && src_[pos_] >= '0' && src_[pos_] <= '9') {
			++pos_;
		}
		if (!at_end() && src_[pos_] == '.') {
			++pos_;
			while (!at_end() && src_[pos_] >= '0' && src_[pos_] <= '9') {
				++pos_;
			}
		}
		if (!at_end() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
			++pos_;
			if (!at_end() && (src_[pos_] == '-' || src_[pos_] == '+')) {
				++pos_;
			}
			while (!at_end() && src_[pos_] >= '0' && src_[pos_] <= '9') {
				++pos_;
			}
		}

		if (pos_ == start) {
			return make_error<err::json_malformed>("Expected a number");
		}

		return src_.substr(start, pos_ - start);
	}

	error_or<bool> parse_bool() {
		skip_whitespace();
		if (src_.compare(pos_, 4u, "true") == 0) {
			pos_ += 4u;
			return true;
		}
		if (src_.compare(pos_, 5u, "false") == 0) {
			pos_ += 5u;
			return false;
		}
		return make_error<err::json_malformed>("Expected a boolean");
	}

	bool consume_null() {
		skip_whitespace();
		if (src_.compare(pos_, 4u, "null") == 0) {
			pos_ += 4u;
			return true;
		}
		return false;
	}

	/**
	 * Steps over an entire value without interpreting it. Used to ignore
	 * members the schema does not declare.
	 */
	error_or<void> skip_value() {
		auto eo_c = peek();
		if (eo_c.is_error()) {
			return std::move(eo_c.get_error());
		}

		auto eo_enter = enter();
		if (eo_enter.is_error()) {
			return std::move(eo_enter.get_error());
		}

		const char c = eo_c.get_value();
		error_or<void> result = make_void();

		if (c == '{' || c == '[') {
			const char close = (c == '{') ? '}' : ']';
			++pos_;
			if (!consume_if(close)) {
				while (true) {
					if (c == '{') {
						auto eo_key = parse_string();
						if (eo_key.is_error()) {
							result = std::move(eo_key.get_error());
							break;
						}
						auto eo_colon = expect(':');
						if (eo_colon.is_error()) {
							result = std::move(eo_colon.get_error());
							break;
						}
					}
					auto eo_inner = skip_value();
					if (eo_inner.is_error()) {
						result = std::move(eo_inner.get_error());
						break;
					}
					if (consume_if(close)) {
						break;
					}
					auto eo_comma = expect(',');
					if (eo_comma.is_error()) {
						result = std::move(eo_comma.get_error());
						break;
					}
				}
			}
		} else if (c == '"') {
			auto eo_s = parse_string();
			if (eo_s.is_error()) {
				result = std::move(eo_s.get_error());
			}
		} else if (consume_null()) {
			// nothing to do
		} else if (c == 't' || c == 'f') {
			auto eo_b = parse_bool();
			if (eo_b.is_error()) {
				result = std::move(eo_b.get_error());
			}
		} else {
			auto eo_n = parse_number_token();
			if (eo_n.is_error()) {
				result = std::move(eo_n.get_error());
			}
		}

		leave();
		return result;
	}

private:
	error_or<uint32_t> parse_hex4() {
		if (pos_ + 4u > src_.size()) {
			return make_error<err::json_malformed>("Truncated \\u escape");
		}
		uint32_t v = 0u;
		for (uint64_t i = 0u; i < 4u; ++i) {
			const char h = src_[pos_ + i];
			uint32_t d;
			if (h >= '0' && h <= '9') {
				d = static_cast<uint32_t>(h - '0');
			} else if (h >= 'a' && h <= 'f') {
				d = static_cast<uint32_t>(h - 'a') + 10u;
			} else if (h >= 'A' && h <= 'F') {
				d = static_cast<uint32_t>(h - 'A') + 10u;
			} else {
				return make_error<err::json_malformed>("Bad hex digit");
			}
			v = (v << 4) | d;
		}
		pos_ += 4u;
		return v;
	}

	static void append_utf8(std::string &out, uint32_t cp) {
		if (cp < 0x80u) {
			out.push_back(static_cast<char>(cp));
		} else if (cp < 0x800u) {
			out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
		} else if (cp < 0x10000u) {
			out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
		} else {
			out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
		}
	}
};

inline void json_append_escaped(std::string &out, std::string_view in) {
	out.push_back('"');
	for (const char c : in) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (static_cast<unsigned char>(c) < 0x20u) {
				char buf[7];
				std::snprintf(buf, sizeof(buf), "\\u%04x",
							  static_cast<unsigned>(static_cast<unsigned char>(c)));
				out += buf;
			} else {
				out.push_back(c);
			}
		}
	}
	out.push_back('"');
}

inline void json_newline_indent(std::string &out, bool pretty, uint64_t level) {
	if (!pretty) {
		return;
	}
	out.push_back('\n');
	for (uint64_t i = 0u; i < level; ++i) {
		out += "  ";
	}
}

/**
 * Renders a native value as JSON text.
 */
template <typename Schema> struct json_encode {
	static_assert(always_false<Schema>,
				  "This schema has no JSON representation");
};

template <> struct json_encode<sch::Void> {
	static error_or<void> apply(const data<sch::Void, enc::Native> &,
								std::string &out, bool, uint64_t) {
		out += "null";
		return make_void();
	}
};

template <> struct json_encode<sch::Bool> {
	static error_or<void> apply(const data<sch::Bool, enc::Native> &in,
								std::string &out, bool, uint64_t) {
		out += in.get() ? "true" : "false";
		return make_void();
	}
};

template <typename K, uint64_t N> struct json_encode<sch::Primitive<K, N>> {
	static error_or<void>
	apply(const data<sch::Primitive<K, N>, enc::Native> &in, std::string &out,
		  bool, uint64_t) {
		using Native = typename native_data_type<sch::Primitive<K, N>>::type;
		const Native v = in.get();

		if constexpr (std::is_floating_point_v<Native>) {
			/**
			 * max_digits10 is the shortest precision that always round trips
			 * back to the same value, so a config survives a save/load cycle.
			 */
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%.*g",
						  std::numeric_limits<Native>::max_digits10,
						  static_cast<double>(v));
			out += buf;
		} else if constexpr (std::is_signed_v<Native>) {
			out += std::to_string(static_cast<int64_t>(v));
		} else {
			out += std::to_string(static_cast<uint64_t>(v));
		}
		return make_void();
	}
};

template <typename S, typename I>
struct json_encode<sch::MixedPrecision<S, I>> {
	static error_or<void>
	apply(const data<sch::MixedPrecision<S, I>, enc::Native> &in,
		  std::string &out, bool pretty, uint64_t level) {
		// A mixed precision value reads as its interface type.
		data<I, enc::Native> widened{in.get()};
		return json_encode<I>::apply(widened, out, pretty, level);
	}
};

template <> struct json_encode<sch::String> {
	static error_or<void> apply(const data<sch::String, enc::Native> &in,
								std::string &out, bool, uint64_t) {
		json_append_escaped(out, in.stl_view());
		return make_void();
	}
};

/**
 * Writes the elements of a flat sequence as a JSON array.
 */
template <typename Inner, typename Container>
inline error_or<void> json_encode_sequence(const Container &in, uint64_t count,
										   std::string &out, bool pretty,
										   uint64_t level) {
	out.push_back('[');
	for (uint64_t i = 0u; i < count; ++i) {
		if (i > 0u) {
			out.push_back(',');
		}
		json_newline_indent(out, pretty, level + 1u);
		auto eov = json_encode<Inner>::apply(in.flat_at(i), out, pretty,
											 level + 1u);
		if (eov.is_error()) {
			return eov;
		}
	}
	if (count > 0u) {
		json_newline_indent(out, pretty, level);
	}
	out.push_back(']');
	return make_void();
}

template <typename T, uint64_t... D>
struct json_encode<sch::FixedArray<T, D...>> {
	static error_or<void>
	apply(const data<sch::FixedArray<T, D...>, enc::Native> &in,
		  std::string &out, bool pretty, uint64_t level) {
		constexpr uint64_t flat = ct_multiply<uint64_t, D...>::value;
		return json_encode_sequence<T>(in, flat, out, pretty, level);
	}
};

template <typename T, uint64_t Dim> struct json_encode<sch::Array<T, Dim>> {
	static error_or<void>
	apply(const data<sch::Array<T, Dim>, enc::Native> &in, std::string &out,
		  bool pretty, uint64_t level) {
		return json_encode_sequence<T>(in, in.size().get(), out, pretty, level);
	}
};

template <typename Inner, uint64_t... D>
struct json_encode<sch::Tensor<Inner, D...>> {
	static error_or<void>
	apply(const data<sch::Tensor<Inner, D...>, enc::Native> &in,
		  std::string &out, bool pretty, uint64_t level) {
		constexpr uint64_t flat = ct_multiply<uint64_t, D...>::value;
		return json_encode_sequence<Inner>(in, flat, out, pretty, level);
	}
};

template <typename... T> struct json_encode<sch::Tuple<T...>> {
	template <uint64_t i = 0u>
	static error_or<void>
	apply_element(const data<sch::Tuple<T...>, enc::Native> &in,
				  std::string &out, bool pretty, uint64_t level) {
		if constexpr (i < sizeof...(T)) {
			if constexpr (i > 0u) {
				out.push_back(',');
			}
			json_newline_indent(out, pretty, level + 1u);
			using Element = typename parameter_pack_type<i, T...>::type;
			auto eov = json_encode<Element>::apply(in.template get<i>(), out,
												   pretty, level + 1u);
			if (eov.is_error()) {
				return eov;
			}
			return apply_element<i + 1u>(in, out, pretty, level);
		} else {
			return make_void();
		}
	}

	static error_or<void> apply(const data<sch::Tuple<T...>, enc::Native> &in,
								std::string &out, bool pretty, uint64_t level) {
		out.push_back('[');
		auto eov = apply_element<0u>(in, out, pretty, level);
		if (eov.is_error()) {
			return eov;
		}
		if constexpr (sizeof...(T) > 0u) {
			json_newline_indent(out, pretty, level);
		}
		out.push_back(']');
		return make_void();
	}
};

template <typename... T, string_literal... Keys>
struct json_encode<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;

	template <uint64_t i = 0u>
	static error_or<void> apply_member(const data<Schema, enc::Native> &in,
									   std::string &out, bool pretty,
									   uint64_t level) {
		if constexpr (i < sizeof...(T)) {
			if constexpr (i > 0u) {
				out.push_back(',');
			}
			json_newline_indent(out, pretty, level + 1u);
			json_append_escaped(
				out, parameter_key_pack_type<i, Keys...>::literal.view());
			out.push_back(':');
			if (pretty) {
				out.push_back(' ');
			}
			using Member = typename parameter_pack_type<i, T...>::type;
			auto eov = json_encode<Member>::apply(
				in.template get_at_index<i>(), out, pretty, level + 1u);
			if (eov.is_error()) {
				return eov;
			}
			return apply_member<i + 1u>(in, out, pretty, level);
		} else {
			return make_void();
		}
	}

	static error_or<void> apply(const data<Schema, enc::Native> &in,
								std::string &out, bool pretty, uint64_t level) {
		out.push_back('{');
		auto eov = apply_member<0u>(in, out, pretty, level);
		if (eov.is_error()) {
			return eov;
		}
		if constexpr (sizeof...(T) > 0u) {
			json_newline_indent(out, pretty, level);
		}
		out.push_back('}');
		return make_void();
	}
};

/**
 * Reads a native value back out of JSON text.
 */
template <typename Schema> struct json_decode {
	static_assert(always_false<Schema>,
				  "This schema has no JSON representation");
};

template <> struct json_decode<sch::Void> {
	static error_or<void> apply(json_parser &p, data<sch::Void, enc::Native> &,
								const codec_config<enc::Json> &) {
		if (!p.consume_null()) {
			return make_error<err::json_type_mismatch>("Expected null");
		}
		return make_void();
	}
};

template <> struct json_decode<sch::Bool> {
	static error_or<void> apply(json_parser &p,
								data<sch::Bool, enc::Native> &out,
								const codec_config<enc::Json> &) {
		auto eo_b = p.parse_bool();
		if (eo_b.is_error()) {
			return std::move(eo_b.get_error());
		}
		out.set(eo_b.get_value());
		return make_void();
	}
};

template <typename K, uint64_t N> struct json_decode<sch::Primitive<K, N>> {
	static error_or<void> apply(json_parser &p,
								data<sch::Primitive<K, N>, enc::Native> &out,
								const codec_config<enc::Json> &) {
		using Native = typename native_data_type<sch::Primitive<K, N>>::type;

		auto eo_tok = p.parse_number_token();
		if (eo_tok.is_error()) {
			return std::move(eo_tok.get_error());
		}
		const std::string_view tok = eo_tok.get_value();

		if constexpr (std::is_floating_point_v<Native>) {
			/**
			 * from_chars for floating point is not available everywhere this
			 * builds, so the token is copied into a terminated buffer and run
			 * through strtod.
			 */
			std::string buf{tok};
			char *end = nullptr;
			const double v = std::strtod(buf.c_str(), &end);
			if (end == buf.c_str()) {
				return make_error<err::json_malformed>("Bad floating point");
			}
			out.set(static_cast<Native>(v));
		} else {
			// Accept a float-looking token for an integer field only when it
			// names an exact integer, rather than silently truncating.
			int64_t sv = 0;
			uint64_t uv = 0u;
			const char *first = tok.data();
			const char *last = tok.data() + tok.size();

			std::from_chars_result r{};
			if constexpr (std::is_signed_v<Native>) {
				r = std::from_chars(first, last, sv);
			} else {
				if (!tok.empty() && tok.front() == '-') {
					return make_error<err::json_type_mismatch>(
						"Negative value for an unsigned field");
				}
				const char *begin = (!tok.empty() && tok.front() == '+')
										? first + 1
										: first;
				r = std::from_chars(begin, last, uv);
			}

			if (r.ec != std::errc{}) {
				return make_error<err::json_malformed>("Bad integer");
			}
			if (r.ptr != last) {
				return make_error<err::json_type_mismatch>(
					"Expected an integer");
			}

			if constexpr (std::is_signed_v<Native>) {
				out.set(static_cast<Native>(sv));
			} else {
				out.set(static_cast<Native>(uv));
			}
		}
		return make_void();
	}
};

template <typename S, typename I>
struct json_decode<sch::MixedPrecision<S, I>> {
	static error_or<void>
	apply(json_parser &p, data<sch::MixedPrecision<S, I>, enc::Native> &out,
		  const codec_config<enc::Json> &cfg) {
		data<I, enc::Native> widened;
		auto eov = json_decode<I>::apply(p, widened, cfg);
		if (eov.is_error()) {
			return eov;
		}
		out.set(widened.get());
		return make_void();
	}
};

template <> struct json_decode<sch::String> {
	static error_or<void> apply(json_parser &p,
								data<sch::String, enc::Native> &out,
								const codec_config<enc::Json> &) {
		auto eo_s = p.parse_string();
		if (eo_s.is_error()) {
			return std::move(eo_s.get_error());
		}
		out.set(std::move(eo_s.get_value()));
		return make_void();
	}
};

/**
 * Reads a JSON array into a fixed length sequence. The element count has to
 * match exactly, since there is nowhere to put a surplus and no default for a
 * shortfall.
 */
template <typename Inner, typename Container>
inline error_or<void> json_decode_fixed_sequence(json_parser &p, Container &out,
												 uint64_t count,
												 const codec_config<enc::Json> &cfg) {
	auto eo_enter = p.enter();
	if (eo_enter.is_error()) {
		return std::move(eo_enter.get_error());
	}

	auto eo_open = p.expect('[');
	if (eo_open.is_error()) {
		p.leave();
		return std::move(eo_open.get_error());
	}

	uint64_t i = 0u;
	if (!p.consume_if(']')) {
		while (true) {
			if (i >= count) {
				p.leave();
				return make_error<err::json_type_mismatch>(
					"JSON array is longer than the schema allows");
			}
			auto eov = json_decode<Inner>::apply(p, out.flat_at(i), cfg);
			if (eov.is_error()) {
				p.leave();
				return eov;
			}
			++i;
			if (p.consume_if(']')) {
				break;
			}
			auto eo_comma = p.expect(',');
			if (eo_comma.is_error()) {
				p.leave();
				return std::move(eo_comma.get_error());
			}
		}
	}

	p.leave();

	if (i != count) {
		return make_error<err::json_type_mismatch>(
			"JSON array is shorter than the schema requires");
	}
	return make_void();
}

template <typename T, uint64_t... D>
struct json_decode<sch::FixedArray<T, D...>> {
	static error_or<void>
	apply(json_parser &p, data<sch::FixedArray<T, D...>, enc::Native> &out,
		  const codec_config<enc::Json> &cfg) {
		constexpr uint64_t flat = ct_multiply<uint64_t, D...>::value;
		return json_decode_fixed_sequence<T>(p, out, flat, cfg);
	}
};

template <typename Inner, uint64_t... D>
struct json_decode<sch::Tensor<Inner, D...>> {
	static error_or<void>
	apply(json_parser &p, data<sch::Tensor<Inner, D...>, enc::Native> &out,
		  const codec_config<enc::Json> &cfg) {
		constexpr uint64_t flat = ct_multiply<uint64_t, D...>::value;
		return json_decode_fixed_sequence<Inner>(p, out, flat, cfg);
	}
};

/**
 * A dynamically sized Array grows to fit whatever the document holds, so only
 * the one dimensional case has a well defined meaning here.
 */
template <typename T, uint64_t Dim> struct json_decode<sch::Array<T, Dim>> {
	static error_or<void> apply(json_parser &p,
								data<sch::Array<T, Dim>, enc::Native> &out,
								const codec_config<enc::Json> &cfg) {
		static_assert(Dim == 1u,
					  "Only a one dimensional Array can be grown from JSON");

		auto eo_enter = p.enter();
		if (eo_enter.is_error()) {
			return std::move(eo_enter.get_error());
		}

		auto eo_open = p.expect('[');
		if (eo_open.is_error()) {
			p.leave();
			return std::move(eo_open.get_error());
		}

		out = data<sch::Array<T, Dim>, enc::Native>{};

		if (!p.consume_if(']')) {
			while (true) {
				data<T, enc::Native> element;
				auto eov = json_decode<T>::apply(p, element, cfg);
				if (eov.is_error()) {
					p.leave();
					return eov;
				}
				auto eo_add = out.add(std::move(element));
				if (eo_add.is_error()) {
					p.leave();
					return eo_add;
				}
				if (p.consume_if(']')) {
					break;
				}
				auto eo_comma = p.expect(',');
				if (eo_comma.is_error()) {
					p.leave();
					return std::move(eo_comma.get_error());
				}
			}
		}

		p.leave();
		return make_void();
	}
};

template <typename... T> struct json_decode<sch::Tuple<T...>> {
	using Schema = sch::Tuple<T...>;

	template <uint64_t i = 0u>
	static error_or<void> apply_element(json_parser &p,
										data<Schema, enc::Native> &out,
										const codec_config<enc::Json> &cfg) {
		if constexpr (i < sizeof...(T)) {
			if constexpr (i > 0u) {
				auto eo_comma = p.expect(',');
				if (eo_comma.is_error()) {
					return std::move(eo_comma.get_error());
				}
			}
			using Element = typename parameter_pack_type<i, T...>::type;
			auto eov =
				json_decode<Element>::apply(p, out.template get<i>(), cfg);
			if (eov.is_error()) {
				return eov;
			}
			return apply_element<i + 1u>(p, out, cfg);
		} else {
			return make_void();
		}
	}

	static error_or<void> apply(json_parser &p, data<Schema, enc::Native> &out,
								const codec_config<enc::Json> &cfg) {
		auto eo_enter = p.enter();
		if (eo_enter.is_error()) {
			return std::move(eo_enter.get_error());
		}

		auto eo_open = p.expect('[');
		if (eo_open.is_error()) {
			p.leave();
			return std::move(eo_open.get_error());
		}

		auto eov = apply_element<0u>(p, out, cfg);
		if (eov.is_error()) {
			p.leave();
			return eov;
		}

		auto eo_close = p.expect(']');
		p.leave();
		if (eo_close.is_error()) {
			return std::move(eo_close.get_error());
		}
		return make_void();
	}
};

template <typename... T, string_literal... Keys>
struct json_decode<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;

	/**
	 * Matches the parsed key against the member literals and decodes into the
	 * one that matches. Reports whether a member took the value, so the caller
	 * can skip over keys the schema does not declare.
	 */
	template <uint64_t i = 0u>
	static error_or<bool> dispatch(std::string_view key, json_parser &p,
								   data<Schema, enc::Native> &out,
								   const codec_config<enc::Json> &cfg,
								   bool (&seen)[sizeof...(T) > 0u ? sizeof...(T) : 1u]) {
		if constexpr (i < sizeof...(T)) {
			if (key == parameter_key_pack_type<i, Keys...>::literal.view()) {
				using Member = typename parameter_pack_type<i, T...>::type;
				auto eov = json_decode<Member>::apply(
					p, out.template get_at_index<i>(), cfg);
				if (eov.is_error()) {
					return std::move(eov.get_error());
				}
				seen[i] = true;
				return true;
			}
			return dispatch<i + 1u>(key, p, out, cfg, seen);
		} else {
			(void)key;
			(void)p;
			(void)out;
			(void)cfg;
			(void)seen;
			return false;
		}
	}

	static error_or<void> apply(json_parser &p, data<Schema, enc::Native> &out,
								const codec_config<enc::Json> &cfg) {
		auto eo_enter = p.enter();
		if (eo_enter.is_error()) {
			return std::move(eo_enter.get_error());
		}

		auto eo_open = p.expect('{');
		if (eo_open.is_error()) {
			p.leave();
			return std::move(eo_open.get_error());
		}

		bool seen[sizeof...(T) > 0u ? sizeof...(T) : 1u] = {};

		if (!p.consume_if('}')) {
			while (true) {
				auto eo_key = p.parse_string();
				if (eo_key.is_error()) {
					p.leave();
					return std::move(eo_key.get_error());
				}
				auto eo_colon = p.expect(':');
				if (eo_colon.is_error()) {
					p.leave();
					return std::move(eo_colon.get_error());
				}

				auto eo_taken =
					dispatch<0u>(eo_key.get_value(), p, out, cfg, seen);
				if (eo_taken.is_error()) {
					p.leave();
					return std::move(eo_taken.get_error());
				}
				if (!eo_taken.get_value()) {
					// A key the schema does not declare: step over its value.
					auto eo_skip = p.skip_value();
					if (eo_skip.is_error()) {
						p.leave();
						return std::move(eo_skip.get_error());
					}
				}

				if (p.consume_if('}')) {
					break;
				}
				auto eo_comma = p.expect(',');
				if (eo_comma.is_error()) {
					p.leave();
					return std::move(eo_comma.get_error());
				}
			}
		}

		p.leave();

		if (cfg.strict) {
			for (uint64_t i = 0u; i < sizeof...(T); ++i) {
				if (!seen[i]) {
					return make_error<err::json_missing_member>();
				}
			}
		}

		return make_void();
	}
};

} // namespace impl

template <typename Schema, typename Encoding> class codec;

/**
 * Translates between a native value and its JSON text.
 */
template <typename Schema> class codec<Schema, enc::Json> final {
private:
	codec_config<enc::Json> cfg_;

public:
	codec() = default;

	explicit codec(const codec_config<enc::Json> &cfg__) : cfg_{cfg__} {}

	KEL_FORBID_COPY(codec);
	KEL_DEFAULT_MOVE(codec);

	error_or<void> encode(const data<Schema, enc::Native> &from,
						  data<Schema, enc::Json> &to) {
		std::string out;
		auto eov = impl::json_encode<Schema>::apply(from, out, cfg_.pretty, 0u);
		if (eov.is_error()) {
			return eov;
		}
		to.set(std::move(out));
		return make_void();
	}

	error_or<void> decode(const data<Schema, enc::Json> &from,
						  data<Schema, enc::Native> &to) {
		impl::json_parser p{from.stl_view(), cfg_.depth};

		auto eov = impl::json_decode<Schema>::apply(p, to, cfg_);
		if (eov.is_error()) {
			return eov;
		}

		// Anything after the top level value means the document is not what it
		// claimed to be, so it is reported rather than quietly ignored.
		p.skip_whitespace();
		if (!p.at_end()) {
			return make_error<err::json_malformed>(
				"Trailing content after the JSON value");
		}

		return make_void();
	}

	void set_pretty(bool pretty) { cfg_.pretty = pretty; }

	void set_strict(bool strict) { cfg_.strict = strict; }
};

} // namespace lbm
} // namespace kel
