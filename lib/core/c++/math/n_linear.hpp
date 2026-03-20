#pragma once

#include "../common.hpp"

namespace kel {
namespace lbm {

template<typename FieldSchema, typename T, uint64_t D>
auto n_linear_interpolate(const saw::data<FieldSchema>& field, const saw::data<sch::Vector<T,D>>& pos){

	// Pos
	auto pos_bound = pos;
	
	// Dimensions
	auto meta = field.dims();

	// Lower Index
	saw::data<sch::FixedArray<sch::UInt64,D>> ind;

	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{D}; ++i){
		// Native Positive i
		auto npos_i = pos.at({i}).get();

		{
			// Ok I want to stay in bounds
			npos_i = std::min(npos_i,meta.at(i).get()-1.0);
			npos_i = std::max(npos_i,1.0);
		}

		// Native Index i
		auto nind_i = static_cast<uint64_t>(std::floor(npos_i))-1ul;

		// Set index to i
		ind.at(i).set(nind_i);
	}
	saw::data<sch::Vector<T,D>> pos_frac;
	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{D}; ++i){
		pos_frac.at({i}) = pos_bound.at({i}) - ind.at(i).template cast_to<T>();
	}

	// Base value
	auto res = field.at(ind);

	constexpr uint64_t d_corners = 1ull << D;
}
}
}
