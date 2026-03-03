#pragma once

namespace kel {
namespace lbm {
template<typename T, uint64_t D>
void blur_mask(saw::data<sch::Array<T,D>>& p_mask){

	saw::data<T> mid{2.0/3.0};
	saw::data<T> edge{1.0/6.0};

	// saw::data<sch::FixedArray<sch::UInt64>,2u> blur_weights{{2.0/3.0},{1.0/6.0}};

	auto meta = p_mask.dims();
	saw::data<sch::Array<T,D>> blurred_mask{meta};

	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{D}; ++i){
		iterator<D>::apply([&](const auto& index){
			blurred_mask.at(index) = p_mask.at(index) * mid;

			if(index.at({i}).get() > 0u){
				auto l_ind = index;
				l_ind.at({i}) = ind.at({i}) - 1u;
				blurred_mask.at(index) = blurred_mask.at(index) + p_mask.at(l_ind) * edge;
			}
			if((index.at({i}).get() + 1u) < meta.at({i})){
				auto r_ind = index;
				r_ind.at({i}) = ind.at({i}) + 1u;
				blurred_mask.at(index) = blurred_mask.at(index) + p_mask.at(r_ind) * edge;
			}
		},{},meta);

		p_mask = blurred_mask;
	}
}
}
}
