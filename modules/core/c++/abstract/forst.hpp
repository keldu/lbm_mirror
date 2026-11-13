#pragma once

#include "data.hpp"
#include "encode.hpp"
#include "schema.hpp"
#include "templates.hpp"

#include <cstddef>
#include <string_view>
#include <cstdint>
#include <bit>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

/**
 * A packed little endian binary encoding for the schema system.
 *
 * Every value sits at a computable byte offset, so reading one member costs no
 * decoding of the members before it. A value is a static section followed by a
 * single dynamic layer. Little endian, byte packed, no alignment and no
 * padding. Fixed size members sit inline; a variable length member spends its
 * static slot on its dimensions as UInt64 each and then an offset, and its
 * bytes live in the dynamic layer.
 *
 * An offset is measured from the byte just past the offset field that holds
 * it, so resolving one is a single add with no layer base to track.
 *
 * This is forstio's Forst format reimplemented against lbm's own schema types
 * (lbm's abstract/ is a private reimplementation of forstio's data/codec, so
 * the two schema vocabularies are distinct C++ types and nothing could be
 * shared), and deliberately cut down to **one dynamic layer**. That makes
 * `String` and `Array<Fixed>` expressible while `Array<String>` and
 * `Array<Array<...>>` are not -- a static_assert says so rather than letting a
 * nested dynamic value mis-encode.
 *
 * Chunk is not handled here: abstract/ sits below chunk.hpp and must not reach
 * up into it. The Chunk layout lives in forst_chunk.hpp, which is what a result
 * writer includes.
 *
 * One limitation shared with enc::Json: a bare `data<Chunk<...>, enc::Forst>`
 * is ambiguous, because chunk.hpp already specialises
 * `data<Chunk<...>, Encode>` for any encoding and neither specialisation is
 * more specialised than the other. Wrap the chunk in a Struct, which is what a
 * field always is in practice.
 *
 * Why it is worth having: a `Struct<Member<Chunk<...>,Key>...>` is entirely
 * fixed size, so its encoding *is* the packed field data, byte for byte, with
 * no framing overhead and no serialisation pass. Results are written as ASCII
 * VTK today, which measures 4-5x larger.
 */
namespace kel {
namespace lbm {

namespace err {
/// The buffer ended before a value that the layout said was there.
struct forst_truncated {
	static constexpr std::string_view description = "Truncated Forst buffer";
	static constexpr bool is_critical = false;
};

/// A length or offset in the buffer does not agree with the buffer itself.
struct forst_malformed {
	static constexpr std::string_view description = "Malformed Forst buffer";
	static constexpr bool is_critical = false;
};
} // namespace err

namespace impl {

/**
 * The layout of a schema.
 *
 * `layers` is dynamicity depth: 0 for something wholly fixed size, and one
 * more for each level of variable length nesting. `static_size` is the bytes
 * the schema occupies in a static section -- for a variable length schema that
 * is the size of its dimensions plus its offset, not of its contents.
 */
template <typename Schema> struct forst_info {
	static_assert(always_false<Schema>, "This schema has no Forst layout");
};

template <> struct forst_info<sch::Void> {
	static constexpr uint64_t layers = 0u;
	static constexpr uint64_t static_size = 0u;
};

template <> struct forst_info<sch::Bool> {
	static constexpr uint64_t layers = 0u;
	static constexpr uint64_t static_size = 1u;
};

template <typename Kind, uint64_t N> struct forst_info<sch::Primitive<Kind, N>> {
	static constexpr uint64_t layers = 0u;
	static constexpr uint64_t static_size = N;
};

/**
 * Stored as its storage type, not its interface type. The whole point of a
 * mixed precision member is that the narrow form is what occupies space.
 */
template <typename Storage, typename Interface>
struct forst_info<sch::MixedPrecision<Storage, Interface>> {
	static constexpr uint64_t layers = forst_info<Storage>::layers;
	static constexpr uint64_t static_size = forst_info<Storage>::static_size;
};

/// Length, then offset.
template <> struct forst_info<sch::String> {
	static constexpr uint64_t layers = 1u;
	static constexpr uint64_t static_size = 16u;
};

template <typename T, uint64_t... D> struct forst_info<sch::FixedArray<T, D...>> {
	static constexpr uint64_t layers = forst_info<T>::layers;
	static constexpr uint64_t static_size =
		ct_multiply<uint64_t, D...>::value * forst_info<T>::static_size;
};

template <typename T, uint64_t... D> struct forst_info<sch::Tensor<T, D...>> {
	static constexpr uint64_t layers = forst_info<T>::layers;
	static constexpr uint64_t static_size =
		ct_multiply<uint64_t, D...>::value * forst_info<T>::static_size;
};

/// One UInt64 per dimension, then the offset.
template <typename T, uint64_t Dim> struct forst_info<sch::Array<T, Dim>> {
	static constexpr uint64_t layers = 1u + forst_info<T>::layers;
	static constexpr uint64_t static_size = 8u * Dim + 8u;
};

template <typename... T> struct forst_info<sch::Tuple<T...>> {
	template <uint64_t i> static constexpr uint64_t max_layers(){
		if constexpr (i < sizeof...(T)){
			using Ti = typename parameter_pack_type<i, T...>::type;
			const uint64_t here = forst_info<Ti>::layers;
			const uint64_t rest = max_layers<i + 1u>();
			return (here > rest) ? here : rest;
		}
		return 0u;
	}

	template <uint64_t i> static constexpr uint64_t sum_static(){
		if constexpr (i < sizeof...(T)){
			using Ti = typename parameter_pack_type<i, T...>::type;
			return forst_info<Ti>::static_size + sum_static<i + 1u>();
		}
		return 0u;
	}

	static constexpr uint64_t layers = max_layers<0u>();
	static constexpr uint64_t static_size = sum_static<0u>();

	/// Byte offset of member i within the static section.
	template <uint64_t i> static constexpr uint64_t offset_of(){
		if constexpr (i == 0u){
			return 0u;
		} else {
			using Prev = typename parameter_pack_type<i - 1u, T...>::type;
			return offset_of<i - 1u>() + forst_info<Prev>::static_size;
		}
	}
};

template <typename... T, string_literal... Keys>
struct forst_info<sch::Struct<sch::Member<T, Keys>...>> {
	using AsTuple = sch::Tuple<T...>;

	static constexpr uint64_t layers = forst_info<AsTuple>::layers;
	static constexpr uint64_t static_size = forst_info<AsTuple>::static_size;

	template <uint64_t i> static constexpr uint64_t offset_of(){
		return forst_info<AsTuple>::template offset_of<i>();
	}
};

} // namespace impl

/**
 * The encoded bytes.
 *
 * Holds its own buffer and copies deeply, like enc::Native. Binary rather than
 * text, so unlike data<Schema, enc::Json> this is a byte vector.
 */
template <typename Schema> class data<Schema, enc::Forst> final {
public:
	using SchemaType = Schema;

private:
	std::vector<uint8_t> value_;

public:
	data() = default;

	explicit data(std::vector<uint8_t> value__) : value_{std::move(value__)} {}

	void set(std::vector<uint8_t> value__) { value_ = std::move(value__); }

	const std::vector<uint8_t> &bytes() const { return value_; }
	std::vector<uint8_t> &bytes() { return value_; }

	const uint8_t *raw() const { return value_.empty() ? nullptr : value_.data(); }
	uint8_t *raw() { return value_.empty() ? nullptr : value_.data(); }

	data<sch::UInt64, enc::Native> size() const { return {value_.size()}; }
};

namespace impl {

/**
 * Little endian is assumed, never converted.
 *
 * wasm is little endian by specification and every host this runs on is too.
 * A big endian target should fail to build rather than silently produce
 * byte-swapped field data that looks almost plausible.
 */
static_assert(std::endian::native == std::endian::little,
			  "Forst assumes a little endian host");

/**
 * Bytes a value needs in the dynamic layer. Zero for anything fixed size.
 *
 * With one layer the contents of a dynamic member are themselves always fixed
 * size, so this never has to recurse into a dynamic child's own dynamic part.
 */
template <typename Schema> struct forst_dynamic_size {
	static uint64_t apply(const data<Schema, enc::Native> &){ return 0u; }
};

template <> struct forst_dynamic_size<sch::String> {
	static uint64_t apply(const data<sch::String, enc::Native> &from){
		return from.size().get();
	}
};

template <typename T, uint64_t Dim> struct forst_dynamic_size<sch::Array<T, Dim>> {
	static uint64_t apply(const data<sch::Array<T, Dim>, enc::Native> &from){
		uint64_t count = 1u;
		for(uint64_t i = 0u; i < Dim; ++i){
			count *= from.meta().flat_at(i).get();
		}
		return count * forst_info<T>::static_size;
	}
};

template <typename T, uint64_t... D>
struct forst_dynamic_size<sch::FixedArray<T, D...>> {
	static uint64_t apply(const data<sch::FixedArray<T, D...>, enc::Native> &from){
		if constexpr (forst_info<T>::layers == 0u){
			return 0u;
		} else {
			uint64_t total = 0u;
			constexpr uint64_t count = ct_multiply<uint64_t, D...>::value;
			for(uint64_t i = 0u; i < count; ++i){
				total += forst_dynamic_size<T>::apply(from.flat_at(i));
			}
			return total;
		}
	}
};

template <typename... T, string_literal... Keys>
struct forst_dynamic_size<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;

	template <uint64_t i>
	static uint64_t step(const data<Schema, enc::Native> &from){
		if constexpr (i < sizeof...(T)){
			using Ti = typename parameter_pack_type<i, T...>::type;
			return forst_dynamic_size<Ti>::apply(from.template get_at_index<i>())
				 + step<i + 1u>(from);
		}
		return 0u;
	}

	static uint64_t apply(const data<Schema, enc::Native> &from){
		return step<0u>(from);
	}
};

/// Writes a value's static bytes at `at`, appending any contents at `dyn`.
template <typename Schema> struct forst_write;

template <typename Kind, uint64_t N> struct forst_write<sch::Primitive<Kind, N>> {
	static void apply(const data<sch::Primitive<Kind, N>, enc::Native> &from,
					  uint8_t *base, uint64_t at, uint64_t &){
		auto value = from.get();
		static_assert(sizeof(value) == N, "Primitive storage must be its width");
		std::memcpy(base + at, &value, N);
	}
};

template <> struct forst_write<sch::Bool> {
	static void apply(const data<sch::Bool, enc::Native> &from, uint8_t *base,
					  uint64_t at, uint64_t &){
		base[at] = from.get() ? 1u : 0u;
	}
};

template <> struct forst_write<sch::Void> {
	static void apply(const data<sch::Void, enc::Native> &, uint8_t *, uint64_t,
					  uint64_t &){}
};

template <> struct forst_write<sch::String> {
	static void apply(const data<sch::String, enc::Native> &from, uint8_t *base,
					  uint64_t at, uint64_t &dyn){
		const uint64_t length = from.size().get();
		std::memcpy(base + at, &length, 8u);

		// Measured from just past the offset field, so resolving it is one add.
		const uint64_t offset = dyn - (at + 16u);
		std::memcpy(base + at + 8u, &offset, 8u);

		if(length > 0u){
			std::memcpy(base + dyn, from.stl_view().data(), length);
		}
		dyn += length;
	}
};

template <typename T, uint64_t Dim> struct forst_write<sch::Array<T, Dim>> {
	static void apply(const data<sch::Array<T, Dim>, enc::Native> &from,
					  uint8_t *base, uint64_t at, uint64_t &dyn){
		static_assert(forst_info<T>::layers == 0u,
			"One dynamic layer only: an Array element must be fixed size. "
			"Array<String> and Array<Array<...>> need a second layer.");

		uint64_t count = 1u;
		for(uint64_t i = 0u; i < Dim; ++i){
			const uint64_t extent = from.meta().flat_at(i).get();
			std::memcpy(base + at + 8u * i, &extent, 8u);
			count *= extent;
		}

		const uint64_t offset = dyn - (at + forst_info<sch::Array<T, Dim>>::static_size);
		std::memcpy(base + at + 8u * Dim, &offset, 8u);

		// Elements are fixed size, so they go straight into the layer with no
		// dynamic part of their own.
		uint64_t unused = 0u;
		const uint64_t stride = forst_info<T>::static_size;
		for(uint64_t i = 0u; i < count; ++i){
			forst_write<T>::apply(from.flat_at(i), base, dyn + i * stride, unused);
		}
		dyn += count * stride;
	}
};

template <typename T, uint64_t... D> struct forst_write<sch::FixedArray<T, D...>> {
	static void apply(const data<sch::FixedArray<T, D...>, enc::Native> &from,
					  uint8_t *base, uint64_t at, uint64_t &dyn){
		constexpr uint64_t count = ct_multiply<uint64_t, D...>::value;
		constexpr uint64_t stride = forst_info<T>::static_size;
		for(uint64_t i = 0u; i < count; ++i){
			forst_write<T>::apply(from.flat_at(i), base, at + i * stride, dyn);
		}
	}
};

template <typename T, uint64_t... D> struct forst_write<sch::Tensor<T, D...>> {
	static void apply(const data<sch::Tensor<T, D...>, enc::Native> &from,
					  uint8_t *base, uint64_t at, uint64_t &dyn){
		constexpr uint64_t count = ct_multiply<uint64_t, D...>::value;
		constexpr uint64_t stride = forst_info<T>::static_size;
		for(uint64_t i = 0u; i < count; ++i){
			forst_write<T>::apply(from.flat_at(i), base, at + i * stride, dyn);
		}
	}
};
template <typename... T, string_literal... Keys>
struct forst_write<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;
	using Info = forst_info<Schema>;

	template <uint64_t i>
	static void step(const data<Schema, enc::Native> &from, uint8_t *base,
					 uint64_t at, uint64_t &dyn){
		if constexpr (i < sizeof...(T)){
			using Ti = typename parameter_pack_type<i, T...>::type;
			forst_write<Ti>::apply(from.template get_at_index<i>(), base,
								   at + Info::template offset_of<i>(), dyn);
			step<i + 1u>(from, base, at, dyn);
		}
	}

	static void apply(const data<Schema, enc::Native> &from, uint8_t *base,
					  uint64_t at, uint64_t &dyn){
		step<0u>(from, base, at, dyn);
	}
};


/**
 * Reads a value's static bytes at `at`.
 *
 * Every read is bounds checked against the buffer rather than trusted: the
 * bytes may have come off a network from a peer that is older, newer, or
 * hostile, and a bad offset would otherwise be an out of bounds read.
 */
template <typename Schema> struct forst_read;

template <typename Kind, uint64_t N> struct forst_read<sch::Primitive<Kind, N>> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::Primitive<Kind, N>, enc::Native> &to){
		if(at + N > size){
			return make_error<err::forst_truncated>();
		}
		typename native_data_type<sch::Primitive<Kind, N>>::type value{};
		std::memcpy(&value, base + at, N);
		to.set(value);
		return make_void();
	}
};

template <> struct forst_read<sch::Bool> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::Bool, enc::Native> &to){
		if(at + 1u > size){
			return make_error<err::forst_truncated>();
		}
		to.set(base[at] != 0u);
		return make_void();
	}
};

template <> struct forst_read<sch::Void> {
	static error_or<void> apply(const uint8_t *, uint64_t, uint64_t,
								data<sch::Void, enc::Native> &){
		return make_void();
	}
};

template <> struct forst_read<sch::String> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::String, enc::Native> &to){
		if(at + 16u > size){
			return make_error<err::forst_truncated>();
		}

		uint64_t length = 0u;
		uint64_t offset = 0u;
		std::memcpy(&length, base + at, 8u);
		std::memcpy(&offset, base + at + 8u, 8u);

		const uint64_t start = at + 16u + offset;
		// Checked as a sum that cannot wrap, so a hostile length plus a
		// hostile offset cannot alias back into the static section.
		if(start > size or length > size - start){
			return make_error<err::forst_malformed>();
		}

		to.set(std::string{reinterpret_cast<const char *>(base + start),
						   static_cast<std::size_t>(length)});
		return make_void();
	}
};

template <typename T, uint64_t Dim> struct forst_read<sch::Array<T, Dim>> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::Array<T, Dim>, enc::Native> &to){
		static_assert(forst_info<T>::layers == 0u,
			"One dynamic layer only: an Array element must be fixed size.");

		constexpr uint64_t slot = forst_info<sch::Array<T, Dim>>::static_size;
		if(at + slot > size){
			return make_error<err::forst_truncated>();
		}

		index_data<Dim> extents;
		uint64_t count = 1u;
		for(uint64_t i = 0u; i < Dim; ++i){
			uint64_t extent = 0u;
			std::memcpy(&extent, base + at + 8u * i, 8u);
			extents.flat_at(i).set(extent);

			// Reject an extent product that would overflow before it is used
			// to size an allocation.
			if(extent != 0u and count > (~uint64_t{0u}) / extent){
				return make_error<err::forst_malformed>();
			}
			count *= extent;
		}

		uint64_t offset = 0u;
		std::memcpy(&offset, base + at + 8u * Dim, 8u);

		constexpr uint64_t stride = forst_info<T>::static_size;
		const uint64_t start = at + slot + offset;
		if(start > size or (stride != 0u and count > (size - start) / stride)){
			return make_error<err::forst_malformed>();
		}

		to = data<sch::Array<T, Dim>, enc::Native>{extents};
		for(uint64_t i = 0u; i < count; ++i){
			auto eov = forst_read<T>::apply(base, size, start + i * stride,
											to.flat_at(i));
			if(eov.is_error()){
				return eov;
			}
		}
		return make_void();
	}
};

template <typename T, uint64_t... D> struct forst_read<sch::FixedArray<T, D...>> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::FixedArray<T, D...>, enc::Native> &to){
		constexpr uint64_t count = ct_multiply<uint64_t, D...>::value;
		constexpr uint64_t stride = forst_info<T>::static_size;
		for(uint64_t i = 0u; i < count; ++i){
			auto eov = forst_read<T>::apply(base, size, at + i * stride,
											to.flat_at(i));
			if(eov.is_error()){
				return eov;
			}
		}
		return make_void();
	}
};

template <typename T, uint64_t... D> struct forst_read<sch::Tensor<T, D...>> {
	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<sch::Tensor<T, D...>, enc::Native> &to){
		constexpr uint64_t count = ct_multiply<uint64_t, D...>::value;
		constexpr uint64_t stride = forst_info<T>::static_size;
		for(uint64_t i = 0u; i < count; ++i){
			auto eov = forst_read<T>::apply(base, size, at + i * stride,
											to.flat_at(i));
			if(eov.is_error()){
				return eov;
			}
		}
		return make_void();
	}
};

template <typename... T, string_literal... Keys>
struct forst_read<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;
	using Info = forst_info<Schema>;

	template <uint64_t i>
	static error_or<void> step(const uint8_t *base, uint64_t size, uint64_t at,
							   data<Schema, enc::Native> &to){
		if constexpr (i < sizeof...(T)){
			using Ti = typename parameter_pack_type<i, T...>::type;
			auto eov = forst_read<Ti>::apply(
				base, size, at + Info::template offset_of<i>(),
				to.template get_at_index<i>());
			if(eov.is_error()){
				return eov;
			}
			return step<i + 1u>(base, size, at, to);
		}
		return make_void();
	}

	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<Schema, enc::Native> &to){
		return step<0u>(base, size, at, to);
	}
};

} // namespace impl

/**
 * Translates between a native value and its Forst bytes.
 */
template <typename Schema> class codec<Schema, enc::Forst> final {
public:
	static_assert(impl::forst_info<Schema>::layers <= 1u,
		"Forst here carries exactly one dynamic layer. A schema needing more "
		"-- Array<String>, Array<Array<...>> -- has to be flattened.");

	codec() = default;

	KEL_FORBID_COPY(codec);
	KEL_DEFAULT_MOVE(codec);

	error_or<void> encode(const data<Schema, enc::Native> &from,
						  data<Schema, enc::Forst> &to){
		constexpr uint64_t static_size = impl::forst_info<Schema>::static_size;
		const uint64_t dynamic_size = impl::forst_dynamic_size<Schema>::apply(from);

		std::vector<uint8_t> buffer;
		try {
			buffer.resize(static_cast<std::size_t>(static_size + dynamic_size));
		} catch (const std::exception &e){
			(void) e;
			return make_error<err::out_of_memory>();
		}

		uint64_t dyn = static_size;
		if(not buffer.empty()){
			impl::forst_write<Schema>::apply(from, buffer.data(), 0u, dyn);
		}

		to.set(std::move(buffer));
		return make_void();
	}

	error_or<void> decode(const data<Schema, enc::Forst> &from,
						  data<Schema, enc::Native> &to){
		const uint64_t size = from.size().get();
		if(size < impl::forst_info<Schema>::static_size){
			return make_error<err::forst_truncated>();
		}
		return impl::forst_read<Schema>::apply(from.raw(), size, 0u, to);
	}
};

} // namespace lbm
} // namespace kel
