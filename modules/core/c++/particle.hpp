#pragma once

#include "component.hpp"

/**
 * Folder aggregator for particle/. Anything that wants the particle layer
 * takes this, rather than reaching for individual headers.
 */
#include "particle/particle.hpp"
#include "particle/aabb.hpp"
#include "particle/porosity.hpp"
#include "particle/mask.hpp"
#include "particle/particle_opa.hpp"
#include "particle/integrate.hpp"
#include "particle/contact.hpp"
#include "particle/cell_list.hpp"
#include "particle/couple.hpp"
#include "particle/voxel_narrow.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct Particle {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::Particle, Encode> {
private:
	kel::lbm::data<sch::Scalar<T>> dt_;
public:
	component(kel::lbm::data<sch::Scalar<T>> dt__):
		dt_{dt__}
	{}

	template<typename ParticleSchema, typename MacroFieldSchema>
	void apply(const kel::lbm::data<ParticleSchema, Encode>& particles, const kel::lbm::data<MacroFieldSchema,Encode>& macros, kel::lbm::data<sch::UInt64> index, kel::lbm::data<sch::UInt64> time_step) const {

		auto& p = particles.at(index);

		// Compute forces

		// Update particle momentum
		verlet_step_lambda_old<T,Descriptor::D>(p,{1.0});  

		// Update porosity over lattice nodes
	
		
	}
};
}
}
