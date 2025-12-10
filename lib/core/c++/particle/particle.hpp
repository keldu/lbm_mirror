#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/data_math.hpp>
#include <forstio/codec/math.hpp>

namespace kel {
namespace lbm {
namespace coll {
struct Spheroid{};
}
namespace sch {
using namespace saw::schema;

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<Vector<T,D>, "position">,
	Member<Vector<T,D>, "position_old">,
	Member<T, "rotation">,
	Member<T, "rotation_old">,

	Member<Vector<T,D>, "acceleration">,
	Member<T, "rotational_acceleration">
>;

template<typename T, uint64_t D>
using ParticleMask = Struct<
	Member<Array<T,D>, "grid">,
	Member<Vector<T,D>, "center_of_mass">
>;

template<typename T>
using ParticleCollisionSpheroid = Struct<
	Member<T, "radius">
>;

template<typename T, uint64_t D>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">,
	Member<ParticleMask<T,D>, "mask">,
	Member<T, "mass">,
	Member<T, "size">
>;
}

template<typename T,uint64_t D>
constexpr auto verlet_step_lambda = [](saw::data<sch::Particle<T,D>>& particle, saw::data<T> time_step_delta){
		auto& body = particle.template get<"rigid_body">();

		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();

		// auto& rot = body.template get<"rotation">();
		auto& acc = body.template get<"acceleration">();

		auto tsd_squared = time_step_delta * time_step_delta;

		saw::data<sch::Vector<T,D>> pos_new;
		// Actual step
		for(uint64_t i = 0u; i < D; ++i){
			pos_new.at({{i}}) = saw::data<T>{2.0} * pos.at({{i}}) - pos_old.at({{i}}) + acc.at({{i}}) * tsd_squared;
		}

		pos_old = pos;
		pos = pos_new;
};

template<typename T, uint64_t D, typename ParticleCollision = sch::ParticleMask<T,D> >
class particle_system {
private:
	saw::data<sch::Array<sch::Particle<T,D>>> particles_;

	void verlet_step(saw::data<sch::Particle<T,D>>& particle, saw::data<T> time_step_delta){
		auto& body = particle.template get<"rigid_body">();

		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();

		// auto& rot = body.template get<"rotation">();
		auto& acc = body.template get<"acceleration">();

		auto tsd_squared = time_step_delta * time_step_delta;

		saw::data<sch::Vector<T,D>> pos_new;
		// Actual step
		for(uint64_t i = 0u; i < D; ++i){
			pos_new.at({{i}}) = saw::data<T>{2.0} * pos.at({{i}}) - pos_old.at({{i}}) + acc.at({{i}}) * tsd_squared;
		}

		pos_old = pos;
		pos = pos_new;
	}
public:
	/**
	 *  Add particle to this class and return an id representing this particle
	 */
	saw::error_or<saw::data<sch::UInt64>> add_particle(saw::data<sch::Particle<T,D>> particle__){
		auto size = particles_.size();
		auto eov = particles_.add(std::move(particle__));
		if(eov.is_error()){
			return std::move(eov.get_error());
		}

		return size;
	}

	/*
	saw::data<sch::Particle<T,D>>& get_particle(saw::data<sch::UInt64> id){
	}
	*/

	void step(saw::data<T> time_step_delta){
		for(saw::data<sch::UInt64> i{0u}; i < particles_.size(); ++i){
			verlet_step(particles_.at(i), time_step_delta);
		}
	}

	template<typename LbmLattice>
	void update_particle_border(saw::data<LbmLattice>& latt){
		(void) latt;
		for(auto& iter : particles_){
			auto& par = iter;

			auto& body = par.template get<"rigid_body">();
			auto& size = par.template get<"size">();


		}
	}

	saw::data<sch::UInt64> size() const {
		return particles_.size();
	}

	/**
	 * Mostly meant for unforeseen use cases.
	 */
	saw::data<sch::Particle<T,D>>& at(saw::data<sch::UInt64> i){
		return particles_.at(i);
	}
};
}
}
