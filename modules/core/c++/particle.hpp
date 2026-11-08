#pragma once

#include "component.hpp"
#include "particle/particle.hpp"

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
