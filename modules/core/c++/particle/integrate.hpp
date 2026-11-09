#pragma once

#include "../component.hpp"
#include "common.hpp"
#include "schema.hpp"
#include "particle.hpp"

namespace kel {
namespace lbm {
/**
 * Zero sub steps means "advance once", not "do not advance".
 *
 * Taking it literally gave ts = 1/0 = inf and a loop that never ran, which
 * silently froze the particle. Several step functions pass {0u}.
 */
inline uint64_t particle_sub_steps(kel::lbm::data<sch::UInt64> sub_steps){
	uint64_t steps = sub_steps.get();
	return (steps == 0u) ? 1u : steps;
}
/**
 * Whether rotation is integrated at all for a given dimension.
 *
 * True in 2D, where the scalar moment of inertia is exact about the single
 * out-of-plane axis and verlet_step_lambda's vector update is the correct
 * integrator.
 *
 * False in 3D, and deliberately so rather than approximately right. Two
 * separate things are missing there: a scalar inertia only describes an
 * isotropic body, and integrating the rotation vector as if it were a
 * displacement cannot represent non-commuting composition -- the gyroscopic
 * term omega x (I omega) is absent entirely. Shipping that would give plausible
 * looking but wrong 3D spin, which is worse than none. Callers can branch on
 * this; a correct 3D path wants a quaternion and an inertia tensor.
 */
template<uint64_t D>
constexpr bool particle_angular_supported = (D == 2u);

/**
 * Turns the accumulated force/torque into acceleration and advances the body.
 *
 * A free function rather than only a component so it is callable from a host
 * test, and so a future stiff-contact scheme can call it from inside its own
 * sub-step loop.
 */
template<typename T, uint64_t D>
void integrate_particle(
	kel::lbm::data<sch::Particle<T,D>>& pi,
	const kel::lbm::data<sch::Vector<T,D>>& body_acceleration,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto& pirb = pi.template get<"rigid_body">();

	const uint64_t steps = particle_sub_steps(sub_steps);

	kel::lbm::data<sch::Scalar<T>> ts;
	ts.at({}).set(
		typename kel::lbm::native_data_type<T>::type(1)
		/ static_cast<typename kel::lbm::native_data_type<T>::type>(steps)
	);

	pirb.template get<"acceleration">() =
		pirb.template get<"force">() / pirb.template get<"mass">()
		+ body_acceleration;

	// See particle_angular_supported for why 3D is excluded rather than
	// approximated.
	if constexpr ( particle_angular_supported<D> ){
		pirb.template get<"angular_acceleration">() =
			pirb.template get<"torque">() / pirb.template get<"inertia">();
	}

	for(uint64_t i{0u}; i < steps; ++i){
		verlet_step_lambda<T,D>(pi,ts);
	}
}

/**
 * Clears the force/torque accumulators. Must run before any contributor.
 */
template<typename T, uint64_t D>
void reset_particle_accumulators(kel::lbm::data<sch::Particle<T,D>>& pi){
	auto& pirb = pi.template get<"rigid_body">();

	auto& force = pirb.template get<"force">();
	for(uint64_t i{0u}; i < D; ++i){
		force.at({{i}}).set(0);
	}

	auto& torque = pirb.template get<"torque">();
	if constexpr ( D == 2u ){
		torque.at({}).set(0);
	}else{
		for(uint64_t i{0u}; i < 3u; ++i){
			torque.at({{i}}).set(0);
		}
	}
}

namespace cmpt {
/**
 * Clears the per-particle force/torque accumulators.
 *
 * Must run before any contributor. Separate from integration because
 * contributors (fluid coupling, contact, body forces) sit between the two.
 */
struct ParticleForceReset {};

/**
 * Turns the accumulated force/torque into acceleration and advances the body.
 *
 * This is the single place that writes acceleration, which is what lets the
 * contributors merely accumulate. Previously each fluid component assigned
 * acceleration directly, so a second contributor silently overwrote the first.
 */
struct ParticleIntegrate {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::ParticleForceReset, Encode> final {
public:
	using Component = cmpt::ParticleForceReset;

	component() = default;

	template<typename ParticleSchema>
	void apply(
		const kel::lbm::data<ParticleSchema,Encode>& pg,
		kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index,
		kel::lbm::data<sch::UInt64> time_step
	) const {
		(void) time_step;

		auto& parts = pg.template get<"particles">();
		reset_particle_accumulators<T,Descriptor::D>(parts.at(index));
	}
};

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::ParticleIntegrate, Encode> final {
public:
	using Component = cmpt::ParticleIntegrate;

private:
	/**
	 * Body acceleration applied to every particle, e.g. gravity. Kept as an
	 * acceleration rather than a force so it does not need the mass.
	 */
	kel::lbm::data<sch::Vector<T,Descriptor::D>> body_acceleration_;

public:
	component():
		body_acceleration_{}
	{}

	component(const kel::lbm::data<sch::Vector<T,Descriptor::D>>& body_acceleration__):
		body_acceleration_{body_acceleration__}
	{}

	template<typename ParticleSchema>
	void apply(
		const kel::lbm::data<ParticleSchema,Encode>& pg,
		kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index,
		kel::lbm::data<sch::UInt64> time_step,
		kel::lbm::data<sch::UInt64> sub_steps
	) const {
		(void) time_step;

		auto& parts = pg.template get<"particles">();
		integrate_particle<T,Descriptor::D>(parts.at(index), body_acceleration_, sub_steps);
	}
};

}
}
