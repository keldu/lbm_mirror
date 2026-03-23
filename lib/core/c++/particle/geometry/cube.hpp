#pragma once

#include "../particle.hpp"

namespace kel {
namespace lbm {
template<typename T, uint64_t D>
class particle_cubic_geometry {
private:
public:
	template<typename MT = T>
	saw::data<sch::ParticleMask<MT,D>> generate_mask(uint64_t resolution, uint64_t bound_nodes = 0u){

		saw::data<sch::ParticleMask<MT,D>> mask;

		auto& grid = mask.template get<"grid">();
		auto& com = mask.template get<"center_of_mass">();


	}
}
}
