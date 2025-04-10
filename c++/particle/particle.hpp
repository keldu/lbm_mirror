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
	Member<Array<T,D>, "mask">
>;

template<typename T, uint64_t D>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">
>;
}

template<typename T, uint64_t D>
class particle_system {
private:
	saw::data<sch::Array<sch::Particle<T,D>>> particles_;
public:

	void step(T time_step){
		for(auto& iter : particles_){
		}
	}
};
}
}
