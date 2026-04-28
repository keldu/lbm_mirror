#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
template<typename D>
saw::data<sch::FixedArray<sch::UInt64,D>> sycl_to_saw_index(const acpp::sycl::id<D>& idx){
	saw::data<sch::FixedArray<sch::UInt64,D>> trans_index;

	for(uint64_t i{0u}; i < D; ++i){
		trans_index.at({{i}}).set(idx[i]);
	}

	return trans_index;
}
}
}
