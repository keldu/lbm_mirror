#pragma once

namespace kel {
namespace lbm {

template<typename T>
class particle_circle_geometry {
private:
	saw::data<T> radius_;
public:
	particle_circle_geometry(saw::data<T> radius__):
		radius_{radius__}
	{}

	template<typename MT>
	saw::data<sch::ParticleMask<MT,2>> generate_mask(uint64_t resolution, uint64_t boundary_nodes = 0) const {
		saw::data<sch::ParticleMask<MT,2>> mask;

		auto& grid = mask.template get<"grid">();

		uint64_t size = resolution + 2*boundary_nodes;
		grid = {{size,size}};

		saw::data<T> radius_squared = radius_ * radius_;



		for(uint64_t i = 0; i < size; ++i){
			for(uint64_t j = 0; j < size; ++j){
				if(i < boundary_nodes || j < boundary_nodes || i >= resolution+boundary_nodes || j >= resolution + boundary_nodes ){
					grid.at({i,j}).set(0);
				}else{

				}
			}
		}

		return mask;
	}
};

}
}
