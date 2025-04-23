#pragma once

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<FixedArray<T,D>, "position">,
	Member<FixedArray<T,D>, "velocity">,
	Member<FixedArray<T,D>, "acceleration">,
	Member<FixedArray<T,D>, "rotate_velocity">,
	Member<FixedArray<T,D>, "rotate_acceleration">
>;

template<typename T, uint64_t D>
using ParticleMask = Struct<
	Member<Array<T,D>, "grid">
>;

template<typename T, uint64_t D>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">,
	Member<ParticleMask<Float32,D>, "mask">
>;
}

template<typename T, uint64_t D, typename Particle>
class particle_system {
private:
	saw::data<sch::Array<Particle, D>> particles_;
public:

	void step(T time_step_delta){
		for(auto& iter : particles_){
			verlet_step(time_step_delta);
		}
	}

	template<typename LbmLattice>
	void update_mask(saw::data<LbmLattice>& latt){
		for(auto& iter : particles_){

		}
	}
};
}
}
