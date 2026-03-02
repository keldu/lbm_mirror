#pragma once

#include "../common.hpp"

namespace kel {
namespace lbm {

template<typename FieldSchema, typename T, uint64_t D>
void n_linear_interpolate(const saw::data<FieldSchema>& field, const saw::data<sch::FixedArray<T,D>>& pos){

	auto pos_bound = pos;
	auto meta = field.dims();
	saw::data<sch::UInt64> ind;
	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{D}; ++i){
		pos_bound.at(i).set(
			std::max(
				std::min(
					meta.at(i).template cast_to<T>().get(),
					pos.at(i).get() + 1.5
				),
				1,
			)
			-1
		);
		ind.at(i) = pos_bound.at(i).template cast_to<sch::UInt64>();
	}
}
}
}
