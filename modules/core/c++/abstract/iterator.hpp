#pragma once

#include "data.hpp"

namespace kel {
namespace lbm {
namespace impl {

template <typename Func, uint64_t... Dims> struct iterator_in_bounds {
private:
	static constexpr uint64_t Rank = sizeof...(Dims);
	static constexpr std::array<uint64_t, Rank> dims_{Dims...};

	template <uint64_t level>
	static error_or<void> apply_i(Func &func, index_data<Rank> &index) {
		if constexpr (level >= Rank) {
			return func(index);
		} else {
			/**
			 * flat_at rather than at, because the index being walked is itself
			 * a rank Rank shape: a bare integer is only a valid index into it
			 * when Rank happens to be one.
			 */
			auto &coord = index.flat_at(level);
			for (coord.set(0u);
				 coord.get() < parameter_pack_value<level, uint64_t, Dims...>::value;
				 ++coord) {
				auto eov = apply_i<level + 1u>(func, index);
				if (eov.is_error()) {
					return eov;
				}
			}
			return make_void();
		}
	}

public:
	static error_or<void> apply(Func &func) {
		index_data<Rank> index;
		return apply_i<0u>(func, index);
	}
};

/**
 * Rank zero still has exactly one position to visit, so the callback runs once
 * with an empty index.
 */
template <typename Func> struct iterator_in_bounds<Func> {
	static error_or<void> apply(Func &func) {
		index_data<0u> index;
		return func(index);
	}
};

} // namespace impl

template <uint64_t... Dims> struct rank_iterator {
	template <typename Func> static error_or<void> in_fixed_bounds(Func &&func) {
		return impl::iterator_in_bounds<Func, Dims...>::apply(func);
	}
};

} // namespace lbm
} // namespace kel
