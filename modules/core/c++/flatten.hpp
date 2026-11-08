#pragma once

#include "abstract/error.hpp"
#include "abstract/data.hpp"

namespace kel {
namespace lbm {
namespace sch {
}

template<typename T, uint64_t D>
struct flatten_index {
public:
	template<uint64_t i>
	static constexpr kel::lbm::data<sch::UInt64> stride(const kel::lbm::data<sch::FixedArray<sch::UInt64,D>>& meta) {
		if constexpr (i > 0u){
			return stride<i-1u>(meta) * meta.at({i-1u});
		}

		return 1u;
	}
private:
	/// 2,3,4 => 2,6,24
	/// i + j * 2 + k * 3*2
	/// 1 + 2 * 2 + 3 * 3*2 = 1+4+18 = 23
	template<uint64_t i>
	static void apply_i(kel::lbm::data<sch::UInt64>& flat_ind, const kel::lbm::data<sch::FixedArray<T,D>>& index, const kel::lbm::data<sch::FixedArray<T,D>>& meta){
		if constexpr ( D > i ) {
			flat_ind = flat_ind + index.at({i}) * stride<i>(meta);
			apply_i<i+1u>(flat_ind,index,meta);
		}
	}
public:
	static kel::lbm::data<T> apply(const kel::lbm::data<sch::FixedArray<T,D>>& index, const kel::lbm::data<sch::FixedArray<T,D>>& meta){
		kel::lbm::data<T> flat_ind{0u};
		apply_i<0u>(flat_ind, index, meta);
		return flat_ind;
	}
};
}
}
