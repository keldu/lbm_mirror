#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, uint64_t D>
saw::data<sch::FixedArray<sch::UInt64,D>> lower_index_from_pos(const saw::data<sch::Vector<T,D>>& pos){
	saw::data<sch::FixedArray<sch::UInt64,D>> ind;

	for(saw::data<sch::UInt64> i{0u}; i < ind.size(); ++i){
		ind.at(i).set(std::max(pos.at(i).template cast_to<sch::UInt64>().get(),0));
	}

	return ind;
}

template<typename T, uint64_t D>
saw::data<sch::FixedArray<sch::UInt64,D>> upper_index_from_pos(const saw::data<sch::Vector<T,D>>& pos){
	auto ind = lower_index_from_pos(pos);

	for(saw::data<sch::UInt64> i{0u}; i < ind.size(); ++i){
		++ind.at(i);
	}
	return ind;
}

}
}
