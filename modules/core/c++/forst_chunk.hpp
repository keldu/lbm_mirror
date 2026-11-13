#pragma once

#include "abstract/forst.hpp"
#include "chunk.hpp"

/**
 * The Forst layout for Chunk.
 *
 * Separate from abstract/forst.hpp because abstract/ sits below chunk.hpp and
 * must not reach up into it. Include this rather than the abstract header when
 * encoding fields.
 *
 * A Chunk is its InnerSchema -- a FixedArray widened by the ghost ring -- so it
 * is entirely fixed size and encodes to exactly the bytes already sitting in
 * flat_data(). That is what makes sending a field snapshot free of any
 * serialisation pass or second buffer, which matters most in a browser tab
 * where a second 18 MiB allocation may simply not be available.
 *
 * The encoded extents are `Sides + 2*Ghost`, not the logical sides. A reader
 * without Ghost cannot index the buffer, which is why the result manifest
 * carries it.
 */
namespace kel {
namespace lbm {
namespace impl {

template <typename T, uint64_t Ghost, uint64_t... Sides>
struct forst_info<sch::Chunk<T, Ghost, Sides...>> {
	using Inner = typename sch::Chunk<T, Ghost, Sides...>::InnerSchema;
	static constexpr uint64_t layers = forst_info<Inner>::layers;
	static constexpr uint64_t static_size = forst_info<Inner>::static_size;
};

template <typename T, uint64_t Ghost, uint64_t... Sides>
struct forst_write<sch::Chunk<T, Ghost, Sides...>> {
	using Schema = sch::Chunk<T, Ghost, Sides...>;
	using Value = typename Schema::StoredValueSchema;

	static void apply(const data<Schema, enc::Native> &from, uint8_t *base,
					  uint64_t at, uint64_t &dyn){
		const uint64_t count = data<Schema, enc::Native>::flat_size().get();
		constexpr uint64_t stride = forst_info<Value>::static_size;
		const auto *elements = from.flat_data();
		for(uint64_t i = 0u; i < count; ++i){
			forst_write<Value>::apply(elements[i], base, at + i * stride, dyn);
		}
	}
};

template <typename T, uint64_t Ghost, uint64_t... Sides>
struct forst_read<sch::Chunk<T, Ghost, Sides...>> {
	using Schema = sch::Chunk<T, Ghost, Sides...>;
	using Value = typename Schema::StoredValueSchema;

	static error_or<void> apply(const uint8_t *base, uint64_t size, uint64_t at,
								data<Schema, enc::Native> &to){
		const uint64_t count = data<Schema, enc::Native>::flat_size().get();
		constexpr uint64_t stride = forst_info<Value>::static_size;
		auto *elements = to.flat_data();
		for(uint64_t i = 0u; i < count; ++i){
			auto eov = forst_read<Value>::apply(base, size, at + i * stride,
												elements[i]);
			if(eov.is_error()){
				return eov;
			}
		}
		return make_void();
	}
};

} // namespace impl
} // namespace lbm
} // namespace kel
