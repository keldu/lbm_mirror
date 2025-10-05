#pragma once

#include "../particle.hpp"

namespace kel {
namespace lbm {

template<typename T>
class particle_circle_geometry {
private:
public:
	particle_circle_geometry()
	{}

	template<typename MT = T>
	saw::data<sch::ParticleMask<MT,2>> generate_mask(uint64_t resolution, uint64_t boundary_nodes = 0) const {
		saw::data<sch::ParticleMask<MT,2>> mask;

		auto& grid = mask.template get<"grid">();
		auto& com = mask.template get<"center_of_mass">();

		//uint64_t rad_i = static_cast<uint64_t>(resolution * radius_.get())+1u;
		uint64_t rad_i = resolution;
		uint64_t size = rad_i + 2*boundary_nodes;
		grid = {{{size,size}}};

		saw::data<T> delta_x{static_cast<typename saw::native_data_type<T>::type>(2.0  / static_cast<double>(rad_i))};
		saw::data<T> center = (saw::data<T>{1.0} + saw::data<T>{2.0} * saw::data<T>{static_cast<saw::native_data_type<T>::type>(boundary_nodes)/rad_i});

		for(uint64_t i = 0; i < size; ++i){
			for(uint64_t j = 0; j < size; ++j){
				grid.at({{i,j}}).set(0);
				if(i >= boundary_nodes and j >= boundary_nodes and i < (rad_i + boundary_nodes) and j < (rad_i + boundary_nodes) ){
					saw::data<T> fi = saw::data<T>{static_cast<saw::native_data_type<T>::type>(0.5+i)} * delta_x - center;
					saw::data<T> fj = saw::data<T>{static_cast<saw::native_data_type<T>::type>(0.5+j)} * delta_x - center;

					auto norm_f_ij = fi*fi + fj*fj;
					if(norm_f_ij.get() <= 1){
						grid.at({{i,j}}).set(1);
					}
				}
			}
		}

		saw::data<sch::Vector<T,2u>> com{{0,0}};
		iterate_over([&](saw::data<saw::FixedArray<sch::UInt64,2u>>& index){

		},{{0u,0u}}, grid.meta());

		return mask;
	}
};

}
}
