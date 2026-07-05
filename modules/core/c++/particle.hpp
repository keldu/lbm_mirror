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
	saw::data<sch::Scalar<T>> dt_;
public:
	component(saw::data<sch::Scalar<T>> dt__):
		dt_{dt__}
	{}

	template<typename ParticleSchema, typename MacroFieldSchema>
	void apply(const saw::data<ParticleSchema, Encode>& particles, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::UInt64> index, saw::data<sch::UInt64> time_step) const {

		auto& p = particles.at(index);

		// Compute forces

		// Update particle velocity
		verlet_step_lambda<T,Descriptor::D>(p,{1.0});  

		// Update porosity over lattice nodes
	
		
	}
};
}
}
