#pragma once

#include "../common.hpp"
#include "../iterator.hpp"

namespace kel {
namespace lbm {
namespace impl {
template<typename FieldSchema, typename T, uint64_t D>
struct n_linear_interpolate_helper final {
	template<uint64_t i = 0u>
	auto apply(const saw::data<FieldSchema>& field, const saw::data<sch::Vector<T,D>>& pos){
		return pos;
	}
};
}

template<typename T, uint64_t D>
saw::data<sch::Tuple<sch::Vector<sch::UInt64,D>,sch::Vector<T,D>>> position_to_index_and_fraction(const saw::data<sch::Vector<T,D>>& pos){
	saw::data<sch::Tuple<sch::Vector<sch::UInt64,D>,sch::Vector<T,D>>> sep;

	auto& ind = sep.template get<0u>();
	auto& frac = sep.template get<1u>();

	auto pos_cpy = pos;
	// Guarantee that the pos is at least 0
	for(uint64_t i = 0u; i < D; ++i){
		pos_cpy.at({{i}}).set(std::max(pos.at({{i}}).get(), static_cast<typename saw::native_data_type<T>::type>(0)));
	}

	// Now we can cast to uint64_t
	for(uint64_t i = 0u; i < D; ++i){
		ind.at({{i}}) = pos_cpy.at({{i}}).template cast_to<sch::UInt64>();
	}

	frac = pos_cpy - ind.template cast_to<T>();

	return sep;
}

template<typename T, uint64_t D>
auto floor_index_from_position(const saw::data<sch::Vector<T,D>>& pos){
	return position_to_index_and_fraction(pos).template get<0u>();
}

template<typename FieldSchema, typename T, uint64_t D>
auto n_linear_interpolate(
		const saw::data<FieldSchema>& field, const saw::data<sch::Vector<T,D>>& pos){

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
	saw::data<typename FieldSchema::ValueType> res;

	// constexpr uint64_t d_corners = 1ul << D;

	saw::data<sch::FixedArray<sch::UInt64,D>> ones_ind;
	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{D}; ++i){
		ones_ind.at({i}).set(1u);
	}

	iterator<D>::apply([&](auto ind){
		// Iterates over (0,0,0) to (1,1,1)
		saw::data<T> weight{1.0};

		for(saw::data<sch::UInt64> d{0u}; d < saw::data<sch::UInt64>{D}; ++d){

			saw::data<T> t = pos_frac.at({d});

			if(ind.at(d).get() == 0u){
				weight = weight * (saw::data<T>{1} - t);
			}else{
				weight = weight * t;
			}
		}

	}, {}, ones_ind);
}

template<typename FieldSchema, typename T>
saw::data<sch::Vector<T,2u>> bilinear_interpolate(const saw::data<FieldSchema>& field, const saw::data<sch::Vector<T,2u>>& pos){
	saw::data<sch::Vector<T,2u>> res;

	{
	}
	return {};
}
}
}
