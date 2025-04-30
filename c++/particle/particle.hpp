#pragma once

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<FixedArray<T,D>, "position">,
	Member<FixedArray<T,D>, "position_old">,
	Member<FixedArray<T,D>, "rotation">,
	Member<FixedArray<T,D>, "rotation_old">,

	Member<FixedArray<T,D>, "acceleration">,
	Member<FixedArray<T,D>, "rotational_acceleration">
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

	void verlet_step(saw::data<sch::Particle<T,D>>& particle, saw::data<T> time_step_delta){
		auto& body = particle.template get<"rigid_body">();

		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();

		auto& rot = body.template get<"rotation">();
		auto& acc = body.template get<"acceleration">();

		auto tsd_squared = time_step_delta * time_step_delta;

		saw::data<sch::FixedArray<T,D>> pos_new;
		// Actual step
		for(uint64_t i = 0u; i < D; ++i){
			pos_new.at({i}) = saw::data<T>{2.0} * pos.at({i}) - pos_old.at({i}) + acc.at({i}) * tsd_squared;
		}

		pos_old = pos;
		pos = pos_new;
	}
public:

	void step(T time_step_delta){
		for(auto& iter : particles_){
			verlet_step(time_step_delta);
		}
	}

	template<typename LbmLattice>
	void update_particle_border(saw::data<LbmLattice>& latt){
		for(auto& iter : particles_){
		}
	}
};
}
}
