#pragma once

#include "../common.hpp"

namespace kel {
namespace lbm {

template<typename T, uint64_t D>
saw::data<sch::FixedArray<sch::UInt64,D>> round_to_unsigned(const saw::data<sch::Vector<T,D>>& inp){
	saw::data<sch::FixedArray<sch::UInt64,D>> rv;
				
	auto zero = static_cast<saw::native_data_type<T>::type>(0);
	auto half = static_cast<saw::native_data_type<T>::type>(0.5);

	for(uint64_t i{0u}; i < D; ++i){
		auto val = inp.at({{i}}).get()+half;
		val = std::max(zero,val);

		rv.at({i}).set(static_cast<uint64_t>(val));
	}

	return rv;
}

}
}
