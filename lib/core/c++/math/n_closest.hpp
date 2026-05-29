#pragma once

#include "../common.hpp"
#include "../iterator.hpp"

namespace kel {
namespace lbm {

template<typename FieldSchema, typename Encode, typename T, uint64_t D>
saw::data<typename FieldSchema::InnerValueSchema> n_closest_read(const saw::data<sch::Ptr<FieldSchema>,Encode>& f, const saw::data<sch::Vector<T,D>>& frac_ind){

	auto shift_frac_ind = frac_ind;
	for(uint64_t i{0u}; i < D; ++i){
		
		shift_frac_ind.at({{i}}) = shift_frac_ind.at({{i}})  + saw::data<T>{0.5};
		if(shift_frac_ind.at({{i}}).get() < 0){
			shift_frac_ind.at({{i}}) = {};
		}
	}

	auto shift_ind = frac_ind.template cast_to<sch::UInt64>();

	return f.at(shift_ind);
}

template<typename FieldSchema, typename Encode, typename T, uint64_t D>
void n_closest_add(saw::data<sch::Ptr<FieldSchema>,Encode>& f, const saw::data<sch::Vector<T,D>>& frac_ind, const saw::data<typename FieldSchema::InnerValueSchema>& val){
	auto shift_frac_ind = frac_ind;
	for(uint64_t i{0u}; i < D; ++i){
		
		shift_frac_ind.at({{i}}) = shift_frac_ind.at({{i}})  + saw::data<T>{0.5};
		if(shift_frac_ind.at({{i}}).get() < 0){
			shift_frac_ind.at({{i}}) = {};
		}
	}

	auto shift_ind = frac_ind.template cast_to<sch::UInt64>();
	auto& f_i = f.at(shift_ind);

	f_i = f_i + val;
}

}
}
