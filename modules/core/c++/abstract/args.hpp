#pragma once

#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>

#include "data.hpp"
#include "data_math.hpp"
#include "encode.hpp"
#include "error.hpp"
#include "schema.hpp"
#include "unit.hpp"

namespace kel {
namespace lbm {

namespace enc {
/**
 * Command line arguments as an encoding.
 *
 * Keeping this schema driven rather than hand parsed is what lets a program's
 * argument list carry the same type information as the rest of the data model
 * -- so a united quantity stays united and a mixed precision value keeps its
 * storage type, instead of every call site reinventing a double.
 */
struct Args {
	static constexpr string_literal name = "enc::Args";
};
}

namespace sch {
/**
 * `program` is argv[0], `args` the named `--name value` pairs, `positionals`
 * the bare words in order.
 */
template<typename InnerSchemaStruct, typename InnerSchemaTuple>
using Args = Struct<
	Member<String, "program">,
	Member<InnerSchemaStruct, "args">,
	Member<InnerSchemaTuple, "positionals">
>;
}

template<typename T>
class data<T, enc::Args> {
	static_assert(always_false<T>,
		"Not supported. Use the sch::Args alias or check how it's defined.");
};

template<typename... Str, string_literal... Lits, typename... Tup>
class data<sch::Args<sch::Struct<sch::Member<Str,Lits>...>, sch::Tuple<Tup...>>, enc::Args> final {
private:
	int argc_;
	char** argv_;

public:
	data(int argc, char** argv):
		argc_{argc},
		argv_{argv}
	{}

	uint64_t size() const {
		if(argc_ < 0){
			return 0u;
		}
		static_assert(sizeof(int) <= sizeof(uint64_t), "int is wider than uint64_t");
		return static_cast<uint64_t>(argc_);
	}

	std::string_view arg_view(uint64_t i) const {
		if(i < size()){
			return std::string_view{argv_[i]};
		}
		return "";
	}
};

namespace impl {

/**
 * Decodes one argument value into one leaf of the schema.
 *
 * Specialize this to teach the argument parser a new leaf type; everything
 * above it -- name matching, positional ordering -- is shared.
 */
template<typename T, typename ToDec>
struct args_decode {
	static_assert(always_false<T,ToDec>,
		"No argument decoding for this schema. Add an args_decode specialization.");
};

template<typename K, uint64_t N, typename ToDec>
struct args_decode<sch::Primitive<K,N>, ToDec> {
	using Schema = sch::Primitive<K,N>;
	using Native = typename native_data_type<Schema>::type;

	static error_or<void> decode(data<Schema, ToDec>& to, std::string_view view){
		if(view.empty()){
			return make_error<err::invalid_state>("Can't decode empty data.");
		}

		if constexpr ( std::is_floating_point_v<Native> ){
			/**
			 * strtod rather than from_chars: the floating point overload of
			 * from_chars is not available on every toolchain this builds with.
			 */
			std::string buf{view};
			char* end = nullptr;
			auto val = std::strtod(buf.c_str(), &end);
			if(end == buf.c_str() || *end != '\0'){
				return make_error<err::invalid_state>(
					std::string{"Expected a number, got '"} + std::string{view} + "'");
			}
			to.set(static_cast<Native>(val));
		}else{
			Native val{};
			auto first = view.data();
			auto last = view.data() + view.size();
			auto res = std::from_chars(first, last, val);
			if(res.ec != std::errc{} || res.ptr != last){
				return make_error<err::invalid_state>(
					std::string{"Expected an integer, got '"} + std::string{view} + "'");
			}
			to.set(val);
		}

		return make_void();
	}
};

template<typename ToDec>
struct args_decode<sch::Bool, ToDec> {
	static error_or<void> decode(data<sch::Bool, ToDec>& to, std::string_view view){
		if(view == "true" || view == "1" || view == "yes" || view == "on"){
			to.set(true);
			return make_void();
		}
		if(view == "false" || view == "0" || view == "no" || view == "off"){
			to.set(false);
			return make_void();
		}
		return make_error<err::invalid_state>(
			std::string{"Expected a boolean, got '"} + std::string{view} + "'");
	}
};

template<typename ToDec>
struct args_decode<sch::String, ToDec> {
	static error_or<void> decode(data<sch::String, ToDec>& to, std::string_view view){
		try {
			to.set(std::string{view});
		} catch(const std::exception&){
			return make_error<err::out_of_memory>();
		}
		return make_void();
	}
};

/**
 * A mixed precision value is written and read as its interface type, so that
 * is what the command line parses into; the narrowing to storage happens in
 * the data class exactly as it does everywhere else.
 */
template<typename S, typename I, typename ToDec>
struct args_decode<sch::MixedPrecision<S,I>, ToDec> {
	using Schema = sch::MixedPrecision<S,I>;

	static error_or<void> decode(data<Schema, ToDec>& to, std::string_view view){
		data<I, ToDec> interface_value;
		auto eov = args_decode<I, ToDec>::decode(interface_value, view);
		if(eov.is_error()){
			return eov;
		}
		to.set(interface_value.get());
		return make_void();
	}
};

/**
 * A united quantity parses as its underlying value and keeps its unit.
 *
 * The unit is a property of the schema, not of the text, so there is nothing
 * to read for it -- but the result is still typed, which is the whole point:
 * a length argument cannot be handed to something expecting a time.
 */
template<typename BaseSchema, typename... Elements, typename ToDec>
struct args_decode<sch::Unit<BaseSchema, Elements...>, ToDec> {
	using Schema = sch::Unit<BaseSchema, Elements...>;

	static error_or<void> decode(data<Schema, ToDec>& to, std::string_view view){
		data<BaseSchema, ToDec> base;
		auto eov = args_decode<BaseSchema, ToDec>::decode(base, view);
		if(eov.is_error()){
			return eov;
		}
		to = data<Schema, ToDec>{base};
		return make_void();
	}
};

/**
 * A Tensor of rank zero is a single value, so it decodes like one. This is
 * what makes sch::Scalar<T> usable as an argument.
 */
template<typename Inner, typename ToDec>
struct args_decode<sch::Tensor<Inner>, ToDec> {
	using Schema = sch::Tensor<Inner>;

	static error_or<void> decode(data<Schema, ToDec>& to, std::string_view view){
		data<Inner, ToDec> inner;
		auto eov = args_decode<Inner, ToDec>::decode(inner, view);
		if(eov.is_error()){
			return eov;
		}
		to.flat_at(0u) = inner;
		return make_void();
	}
};

template<typename... Str, string_literal... Lits, typename... Tup, typename ToDec>
struct args_decode<sch::Args<sch::Struct<sch::Member<Str,Lits>...>, sch::Tuple<Tup...>>, ToDec> {
	using Schema = sch::Args<
		sch::Struct<sch::Member<Str,Lits>...>,
		sch::Tuple<Tup...>
	>;

	using SchemaStruct = sch::Struct<sch::Member<Str,Lits>...>;
	using SchemaTuple = sch::Tuple<Tup...>;

	template<uint64_t i>
	static error_or<void> decode_struct_member(
		data<SchemaStruct, ToDec>& to,
		std::string_view name,
		std::string_view value
	){
		if constexpr ( i < sizeof...(Str) ){
			static constexpr string_literal Literal = parameter_key_pack_type<i,Lits...>::literal;
			using StrT = typename parameter_pack_type<i,Str...>::type;

			if(Literal.view() == name){
				return args_decode<StrT, ToDec>::decode(to.template get<Literal>(), value);
			}

			return decode_struct_member<i+1u>(to, name, value);
		}else{
			(void) to;
			(void) value;
			/**
			 * Named rather than generic: an unrecognised flag is almost always
			 * a typo, and saying which one turns a puzzled re-run into a fix.
			 */
			return make_error<err::not_found>(
				std::string{"Unknown argument --"} + std::string{name});
		}
	}

	template<uint64_t i>
	static error_or<void> decode_tuple_member(
		data<SchemaTuple, ToDec>& to,
		uint64_t ptr_i,
		std::string_view value
	){
		if constexpr ( i < sizeof...(Tup) ){
			if( i == ptr_i ){
				using TupT = typename parameter_pack_type<i,Tup...>::type;
				return args_decode<TupT, ToDec>::decode(to.template get<i>(), value);
			}
			return decode_tuple_member<i+1u>(to, ptr_i, value);
		}else{
			(void) to;
			(void) ptr_i;
			return make_error<err::invalid_state>(
				std::string{"Too many positional arguments, did not expect '"}
				+ std::string{value} + "'");
		}
	}

	static error_or<void> decode(
		const data<Schema, enc::Args>& from,
		data<Schema, ToDec>& to
	){
		if(from.size() == 0u){
			return make_error<err::invalid_state>("No arguments at all, not even argv[0]");
		}

		to.template get<"program">().set(std::string{from.arg_view(0u)});
		uint64_t tuple_pos = 0u;

		for(uint64_t i = 1u; i < from.size(); ++i){
			auto view = from.arg_view(i);

			if(view.starts_with("--")){
				view.remove_prefix(2u);
				++i;

				if(i >= from.size()){
					return make_error<err::invalid_state>(
						std::string{"Missing value for --"} + std::string{view});
				}

				auto eov = decode_struct_member<0u>(
					to.template get<"args">(), view, from.arg_view(i));
				if(eov.is_error()){
					return eov;
				}
				continue;
			}

			auto eov = decode_tuple_member<0u>(
				to.template get<"positionals">(), tuple_pos, view);
			if(eov.is_error()){
				return eov;
			}
			++tuple_pos;
		}

		if(tuple_pos != sizeof...(Tup)){
			return make_error<err::invalid_state>(
				std::string{"Expected "} + std::to_string(sizeof...(Tup))
				+ " positional arguments, got " + std::to_string(tuple_pos));
		}

		return make_void();
	}
};

} // namespace impl

template<typename Schema, typename Encoding> class codec;

template<typename Schema>
class codec<Schema, enc::Args> final {
public:
	codec() = default;

	template<typename ToDec>
	error_or<void> decode(const data<Schema, enc::Args>& from, data<Schema, ToDec>& to){
		return impl::args_decode<Schema,ToDec>::decode(from,to);
	}
};

/**
 * Parses argv straight into a schema instance.
 */
template<typename ArgSch>
error_or<void> parse_args(data<ArgSch>& dec_dat, int argc, char** argv){
	data<ArgSch, enc::Args> arg_dat{argc, argv};
	codec<ArgSch, enc::Args> arg_cod;
	return arg_cod.decode(arg_dat, dec_dat);
}

} // namespace lbm
} // namespace kel
