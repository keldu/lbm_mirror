#pragma once

#include "../common.hpp"
#include "../iterator.hpp"

namespace kel {
namespace lbm {

template<typename FieldSchema, typename T, uint64_t D>
auto n_closest_read(saw::data<FieldSchema>, const saw::data<sch::Vector<T,D>>& frac_ind){

	auto shift_frac_ind = frac_ind;
	for(uint64_t i{0u}; i < D; ++i){
		
		shift_frac_ind.at({{i}}) = shift_frac_ind.at({{i}})  + saw::data<T>{0.5};
		if(shift_frac_ind.at({{i}}).get() < 0){
			shift_frac_ind.at({{i}}) = {};
		}
	}

	auto shift_ind = frac_ind.template cast_to<sch::UInt64>();
}
}
}
