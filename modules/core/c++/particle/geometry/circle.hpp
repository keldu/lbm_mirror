#pragma once

#include "../particle.hpp"

namespace kel {
namespace lbm {

template<typename T, uint64_t D>
class particle_circle_geometry {
private:
public:
	particle_circle_geometry()
	{}

	template<typename MT = T>
	saw::data<sch::ParticleMask<MT,D>> generate_mask(uint64_t resolution, uint64_t boundary_nodes = 0) const {
		saw::data<sch::ParticleMask<MT,D>> mask;

		auto& grid = mask.template get<"grid">();
		auto& com = mask.template get<"center_of_mass">();
		com = {};

		//uint64_t rad_i = static_cast<uint64_t>(resolution * radius_.get())+1u;
		uint64_t diameter_i = resolution;
		uint64_t size = diameter_i + 2*boundary_nodes;
		grid = {{{size,size}}};

		saw::data<T> delta_x{static_cast<typename saw::native_data_type<T>::type>(2.0  / static_cast<double>(diameter_i))};
		saw::data<T> center = (saw::data<T>{1.0} + saw::data<T>{2.0} * saw::data<T>{static_cast<saw::native_data_type<T>::type>(boundary_nodes)/diameter_i});

		saw::data<sch::FixedArray<sch::UInt64,D>> boundary_arr;
		for(uint64_t i = 0u; i < D; ++i){
			boundary_arr.set(boundary_nodes);
		}

		iterator<D>::apply([&](const auto& index){
			grid.at(index).set(0);
		}, {}, grid.dims());
		
		iterator<D>::apply([&](const auto& index){
			saw::data<sch::Vector<T,D>> f_ind;
			for(uint64_t i = 0u; i < D; ++i){
				f_ind.at({{i}}) = (index.at({{i}}) + 0.5) * delta_x - center;
			}

			auto norm_f_ind = saw::math::norm(f_ind);
		}, {}, grid.dims(),boundary_arr);

		for(uint64_t i = 0; i < size; ++i){
			for(uint64_t j = 0; j < size; ++j){
				grid.at({{i,j}}).set(0);
				if(i >= boundary_nodes and j >= boundary_nodes and i < (diameter_i + boundary_nodes) and j < (diameter_i + boundary_nodes) ){
					saw::data<T> fi = saw::data<T>{static_cast<saw::native_data_type<T>::type>(0.5+i)} * delta_x - center;
					saw::data<T> fj = saw::data<T>{static_cast<saw::native_data_type<T>::type>(0.5+j)} * delta_x - center;

					auto norm_f_ij = fi*fi + fj*fj;
					if(norm_f_ij.get() <= 1){
						grid.at({{i,j}}).set(1);
					}
				}
			}
		}

		// I don't fully remember what this did
		saw::data<T> total_mass{};
		iterator::apply<D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,D>>& index){
			auto ind_vec = saw::math::vectorize_data(index).template cast_to<T>();
			for(uint64_t i{0u}; i < D; ++i){
				ind_vec.at({{i}}) = ind_vec.at({{i}}) * grid.at(index);
			}
			com = com + ind_vec;

			total_mass = total_mass + grid.at(index);
		},{}, grid.dims());

		for(uint64_t i{0u}; i < D; ++i){
			com.at({{i}}) = com.at({{i}}) / total_mass;
		}

		return mask;
	}
};

}
}
