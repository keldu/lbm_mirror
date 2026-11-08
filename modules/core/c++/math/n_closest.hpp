#pragma once

#include "../common.hpp"
#include "../iterator.hpp"

namespace kel {
namespace lbm {

template<typename FieldSchema, typename Encode, typename T, uint64_t D>
kel::lbm::data<typename FieldSchema::StoredValueSchema> n_closest_read(const kel::lbm::data<sch::Ptr<FieldSchema>,Encode>& f, const kel::lbm::data<sch::Vector<T,D>>& frac_ind){

	auto shift_frac_ind = frac_ind;
	for(uint64_t i{0u}; i < D; ++i){
		
		shift_frac_ind.at({{i}}) = shift_frac_ind.at({{i}})  + kel::lbm::data<T>{0.5};
		if(shift_frac_ind.at({{i}}).get() < 0){
			shift_frac_ind.at({{i}}) = {};
		}
	}

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> shift_ind;
	for(uint64_t i{0u}; i < D; ++i){
		shift_ind.at({i}) = frac_ind.at({{i}}).template cast_to<sch::UInt64>();
	}

	return f.at(shift_ind);
}

template<typename FieldSchema, typename Encode, typename T, uint64_t D>
void n_closest_add(const kel::lbm::data<sch::Ptr<FieldSchema>,Encode>& f, const kel::lbm::data<sch::Vector<T,D>>& frac_ind, const kel::lbm::data<typename FieldSchema::StoredValueSchema>& val){
	auto shift_frac_ind = frac_ind;
	for(uint64_t i{0u}; i < D; ++i){
		
		shift_frac_ind.at({{i}}) = shift_frac_ind.at({{i}})  + kel::lbm::data<T>{0.5};
		if(shift_frac_ind.at({{i}}).get() < 0){
			shift_frac_ind.at({{i}}) = {};
		}
	}

	auto f_meta = f.meta();
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> shift_ind;
	for(uint64_t i{0u}; i < D; ++i){
		shift_ind.at({i}) = frac_ind.at({{i}}).template cast_to<sch::UInt64>();
		if(shift_ind.at({i}) < f_meta.at({i})){
			shift_ind.at({i}) = f_meta.at({i}) - 1u;
		}
	}
	auto& f_i = f.at(shift_ind);

	f_i = f_i + val;
}

}
}
