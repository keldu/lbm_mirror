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
	const kel::lbm::data<sch::LbmAcceleration<sch::Vector<T,D>>>& body_acceleration,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto& pirb = pi.template get<"rigid_body">();

	const uint64_t steps = particle_sub_steps(sub_steps);

	// A sub step is a lattice time, not a bare number.
	kel::lbm::data<sch::LbmSecond<sch::Scalar<T>>> ts;
	ts.at({}).set(
		typename kel::lbm::native_data_type<T>::type(1)
		/ static_cast<typename kel::lbm::native_data_type<T>::type>(steps)
	);

	/**
	 * The seam between the typed and untyped halves. Force and mass carry no
	 * units yet -- there is no mass base unit -- so their quotient is a bare
	 * vector and the acceleration unit is attached here, deliberately and in
	 * one place, rather than being acquired silently at an assignment.
	 */
	pirb.template get<"acceleration">() =
		kel::lbm::data<sch::LbmAcceleration<sch::Vector<T,D>>>{
			pirb.template get<"force">() / pirb.template get<"mass">()}
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
 * Whether rotation about a single FIXED axis can be integrated exactly.
 *
 * True in 3D as well as 2D, unlike particle_angular_supported. Constraining
 * the body to one axis does not paper over the three problems listed there --
 * it removes all of them:
 *
 *  - Rotation composition only fails to commute when the axis moves. If the
 *    angular acceleration is always parallel to a fixed axis and the rotation
 *    starts parallel to it, the rotation vector stays parallel to it for all
 *    time, so verlet_step_lambda's component-wise update is exact rather than
 *    a small-angle approximation.
 *  - omega x (I omega) vanishes when omega lies along a principal axis. Every
 *    axis of a sphere is principal, and a rotor symmetric about its own axis
 *    has that axis principal. The term is genuinely zero, not neglected.
 *  - A scalar inertia is exactly the moment about that one axis.
 *
 * This is the wind turbine case: a body pinned in place, free to spin about
 * one axis and nothing else.
 */
template<uint64_t D>
constexpr bool particle_fixed_axis_supported = (D == 2u or D == 3u);

namespace impl {
/**
 * The spin axis has the same schema as the rotation it drives: a signed scalar
 * in 2D (there is only the out-of-plane direction), a unit vector in 3D.
 */
template<typename T, uint64_t D>
using spin_axis_schema = typename sch::impl::rotation_type_helper<T,D>::Schema;

template<typename T, uint64_t D>
kel::lbm::data<spin_axis_schema<T,D>> normalise_spin_axis(
	const kel::lbm::data<spin_axis_schema<T,D>>& axis
){
	if constexpr ( D == 2u ){
		kel::lbm::data<spin_axis_schema<T,D>> out;
		const auto a = axis.at({}).get();
		out.at({}).set((a < 0) ? -1 : 1);
		return out;
	}else{
		// math::normalize returns a zero vector unchanged rather than NaN, so
		// a caller that forgets the axis gets a body that never spins instead
		// of one that poisons the whole simulation.
		return kel::lbm::math::normalize(axis);
	}
}
}

/**
 * Projects the accumulated torque onto a fixed axis and advances the body
 * about it, holding the position fixed.
 *
 * The position is pinned by zeroing the acceleration, not by writing the
 * position back each step. Position-Verlet is x_new = 2x - x_old + a dt^2, so
 * with x_old == x at construction and a == 0 the body stays exactly where it
 * started, every step, bitwise, with no drift to correct for.
 */
template<typename T, uint64_t D>
void integrate_particle_fixed_axis(
	kel::lbm::data<sch::Particle<T,D>>& pi,
	const kel::lbm::data<impl::spin_axis_schema<T,D>>& axis,
	kel::lbm::data<sch::UInt64> sub_steps
){
	static_assert(particle_fixed_axis_supported<D>, "Fixed axis spin needs D == 2 or D == 3");

	auto& pirb = pi.template get<"rigid_body">();

	const uint64_t steps = particle_sub_steps(sub_steps);

	// A sub step is a lattice time, not a bare number.
	kel::lbm::data<sch::LbmSecond<sch::Scalar<T>>> ts;
	ts.at({}).set(
		typename kel::lbm::native_data_type<T>::type(1)
		/ static_cast<typename kel::lbm::native_data_type<T>::type>(steps)
	);

	// Pinned: the accumulated force is deliberately ignored.
	auto& acc = pirb.template get<"acceleration">();
	for(uint64_t i{0u}; i < D; ++i){
		acc.at({{i}}).set(0);
	}

	auto& torque  = pirb.template get<"torque">();
	auto& inertia = pirb.template get<"inertia">();
	auto& ang_acc = pirb.template get<"angular_acceleration">();

	if constexpr ( D == 2u ){
		ang_acc.at({}).set(
			axis.at({}).get() * torque.at({}).get() / inertia.at({}).get()
		);
	}else{
		// tau_axis = axis . torque, then alpha = axis * (tau_axis / I). Any
		// torque component off the axis is reacted by the mounting and does
		// not accelerate the rotor.
		const auto tau_axis = kel::lbm::math::dot(axis,torque).at({}).get();
		const auto alpha = tau_axis / inertia.at({}).get();
		for(uint64_t i{0u}; i < 3u; ++i){
			ang_acc.at({{i}}).set(axis.at({{i}}).get() * alpha);
		}
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

/**
 * Integrates a body pinned in place and free to spin about one fixed axis.
 *
 * The rotor case: the fluid torque projected onto the axis spins it, the
 * position never moves, and angular velocity is an output. Unlike
 * ParticleIntegrate this works in 3D -- see particle_fixed_axis_supported for
 * why the constraint makes that exact rather than approximate.
 */
struct ParticleAxisIntegrate {};
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
	kel::lbm::data<sch::LbmAcceleration<sch::Vector<T,Descriptor::D>>> body_acceleration_;

public:
	component():
		body_acceleration_{}
	{}

	component(const kel::lbm::data<sch::LbmAcceleration<sch::Vector<T,Descriptor::D>>>& body_acceleration__):
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

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::ParticleAxisIntegrate, Encode> final {
public:
	using Component = cmpt::ParticleAxisIntegrate;

private:
	/**
	 * Normalised once here rather than per step, so the kernel never divides
	 * by a length and a caller cannot change the axis mid-run.
	 */
	kel::lbm::data<impl::spin_axis_schema<T,Descriptor::D>> axis_;

public:
	component(const kel::lbm::data<impl::spin_axis_schema<T,Descriptor::D>>& axis__):
		axis_{impl::normalise_spin_axis<T,Descriptor::D>(axis__)}
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
		integrate_particle_fixed_axis<T,Descriptor::D>(parts.at(index), axis_, sub_steps);
	}
};

}
}
