#include <forstio/test/suite.hpp>

#include <cmath>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 3u;

/**
 * The fixed-axis rotor: a body pinned in place, free to spin about one axis.
 *
 * particle_angular_supported<3> is false and stays false -- general 3D spin
 * needs a quaternion and an inertia tensor. These cover the constrained case
 * instead, which is exact rather than approximate because the axis never
 * moves. See particle_fixed_axis_supported for the argument.
 */

auto make_rotor(double radius = 2.0, double density = 1.0){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>> r;
	kel::lbm::data<sch::Scalar<sch::T>> d;
	r.at({}).set(radius);
	d.at({}).set(density);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,1u>(r,d,{8u});
}

template<typename G>
auto& body(G& g){
	return g.template get<"particles">().at({0u}).template get<"rigid_body">();
}

template<typename G>
void place_at_rest(G& g, double x, double y, double z){
	auto& rb = body(g);
	auto& pos = rb.template get<"position">();
	auto& old = rb.template get<"position_old">();
	const double v[3] = {x,y,z};
	for(uint64_t i = 0u; i < 3u; ++i){
		pos.at({{i}}).set(v[i]);
		// position_old == position is what makes the pin exact.
		old.at({{i}}).set(v[i]);
	}
	auto& rot = rb.template get<"rotation">();
	auto& rot_old = rb.template get<"rotation_old">();
	auto& ang = rb.template get<"angular_acceleration">();
	for(uint64_t i = 0u; i < 3u; ++i){
		rot.at({{i}}).set(0);
		rot_old.at({{i}}).set(0);
		ang.at({{i}}).set(0);
	}
}

kel::lbm::data<sch::Vector<sch::T,3u>> vec3(double x, double y, double z){
	kel::lbm::data<sch::Vector<sch::T,3u>> v;
	v.at({{0u}}).set(x);
	v.at({{1u}}).set(y);
	v.at({{2u}}).set(z);
	return v;
}

template<typename G>
void set_torque(G& g, double x, double y, double z){
	auto& t = body(g).template get<"torque">();
	t.at({{0u}}).set(x);
	t.at({{1u}}).set(y);
	t.at({{2u}}).set(z);
}

SAW_TEST("Rotor 3D / fixed axis is supported where general spin is not"){
	using namespace kel;
	static_assert(not lbm::particle_angular_supported<3u>,
		"General 3D spin must stay gated off");
	static_assert(lbm::particle_fixed_axis_supported<3u>,
		"Fixed axis spin must be available in 3D");
	static_assert(lbm::particle_fixed_axis_supported<2u>,
		"Fixed axis spin must also cover 2D");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Rotor 3D / position is pinned bitwise under load"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 7.0, 11.0, 13.0);

	auto& rb = body(g);
	// A force far larger than anything the fluid would produce.
	auto& f = rb.template get<"force">();
	f.at({{0u}}).set(1.0e6);
	f.at({{1u}}).set(-5.0e5);
	f.at({{2u}}).set(2.5e5);

	set_torque(g, 0.0, 3.0, 0.0);

	auto axis = vec3(0.0,1.0,0.0);
	auto& pi = g.template get<"particles">().at({0u});

	for(uint64_t s = 0u; s < 64u; ++s){
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {4u});
	}

	auto& pos = rb.template get<"position">();
	// Exact equality, not a tolerance: 2x - x_old + 0 reproduces x bitwise.
	SAW_EXPECT(pos.at({{0u}}).get() == 7.0, "x must not drift");
	SAW_EXPECT(pos.at({{1u}}).get() == 11.0, "y must not drift");
	SAW_EXPECT(pos.at({{2u}}).get() == 13.0, "z must not drift");
}

SAW_TEST("Rotor 3D / only the on-axis torque component spins it"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 0.0, 0.0, 0.0);

	// Mostly off-axis: only the y component should drive a y-axis rotor.
	set_torque(g, 9.0, 2.0, -4.0);

	auto axis = vec3(0.0,1.0,0.0);
	auto& pi = g.template get<"particles">().at({0u});
	lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {1u});

	auto& ang = body(g).template get<"angular_acceleration">();
	const double inertia = body(g).template get<"inertia">().at({}).get();

	SAW_EXPECT(std::abs(ang.at({{0u}}).get()) < 1e-12, "No spin off the axis in x");
	SAW_EXPECT(std::abs(ang.at({{2u}}).get()) < 1e-12, "No spin off the axis in z");
	SAW_EXPECT(std::abs(ang.at({{1u}}).get() - (2.0 / inertia)) < 1e-12,
		"On-axis component must give tau/I");
}

SAW_TEST("Rotor 3D / rotation stays parallel to the axis"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 0.0, 0.0, 0.0);

	// A deliberately tilted, non-unit axis: the component must normalise it.
	auto axis = vec3(1.0, 2.0, -2.0);
	auto& pi = g.template get<"particles">().at({0u});

	for(uint64_t s = 0u; s < 128u; ++s){
		set_torque(g, 0.5, 1.0, 0.25);
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {2u});
	}

	auto& rot = body(g).template get<"rotation">();

	// |rot| must equal |rot . axis_hat|: no component ever grows off-axis.
	const double n = std::sqrt(1.0 + 4.0 + 4.0);
	const double ax[3] = {1.0/n, 2.0/n, -2.0/n};

	double along = 0.0;
	double len2 = 0.0;
	for(uint64_t i = 0u; i < 3u; ++i){
		along += rot.at({{i}}).get() * ax[i];
		len2  += rot.at({{i}}).get() * rot.at({{i}}).get();
	}
	const double len = std::sqrt(len2);

	SAW_EXPECT(len > 1e-9, "The rotor should actually have turned");
	SAW_EXPECT(std::abs(len - std::abs(along)) < 1e-9,
		"Rotation must stay parallel to the spin axis");
}

SAW_TEST("Rotor 3D / constant torque spins up linearly"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 0.0, 0.0, 0.0);

	auto axis = vec3(0.0,0.0,1.0);
	auto& pi = g.template get<"particles">().at({0u});
	auto& rb = body(g);

	const double inertia = rb.template get<"inertia">().at({}).get();
	const double tau = 3.0;

	// One sub step, so a step is a unit of time and omega is rot - rot_old.
	auto omega_z = [&](){
		return rb.template get<"rotation">().at({{2u}}).get()
			- rb.template get<"rotation_old">().at({{2u}}).get();
	};

	for(uint64_t s = 0u; s < 8u; ++s){
		set_torque(g, 0.0, 0.0, tau);
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {1u});
	}
	const double w8 = omega_z();

	for(uint64_t s = 0u; s < 8u; ++s){
		set_torque(g, 0.0, 0.0, tau);
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {1u});
	}
	const double w16 = omega_z();

	SAW_EXPECT(w8 > 0.0, "A positive torque must spin it positively");
	// Constant angular acceleration: omega is linear in step count, so
	// doubling the steps doubles omega.
	SAW_EXPECT(std::abs(w16 - 2.0 * w8) < 1e-9 * std::abs(w16),
		"Angular velocity must grow linearly under constant torque");
	SAW_EXPECT(std::abs(w8 - 8.0 * (tau / inertia)) < 1e-9,
		"Angular velocity must match tau/I per step");
}

SAW_TEST("Rotor 3D / no torque means no spin"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 0.0, 0.0, 0.0);
	set_torque(g, 0.0, 0.0, 0.0);

	auto axis = vec3(0.0,1.0,0.0);
	auto& pi = g.template get<"particles">().at({0u});

	for(uint64_t s = 0u; s < 32u; ++s){
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {4u});
	}

	auto& rot = body(g).template get<"rotation">();
	for(uint64_t i = 0u; i < 3u; ++i){
		SAW_EXPECT(rot.at({{i}}).get() == 0.0, "A torque free rotor must not turn");
	}
}

SAW_TEST("Rotor 3D / a zero axis is inert rather than NaN"){
	using namespace kel;

	auto g = make_rotor();
	place_at_rest(g, 0.0, 0.0, 0.0);
	set_torque(g, 1.0, 1.0, 1.0);

	// math::normalize returns a zero vector unchanged, so this must produce a
	// body that never spins -- not one that poisons the run with NaN.
	auto axis = vec3(0.0,0.0,0.0);
	auto& pi = g.template get<"particles">().at({0u});

	for(uint64_t s = 0u; s < 8u; ++s){
		lbm::integrate_particle_fixed_axis<sch::T,D>(pi, axis, {2u});
	}

	auto& rot = body(g).template get<"rotation">();
	for(uint64_t i = 0u; i < 3u; ++i){
		const double v = rot.at({{i}}).get();
		SAW_EXPECT(v == v, "Rotation must not be NaN");
		SAW_EXPECT(std::abs(v) < 1e-12, "A zero axis must not spin the body");
	}
}
}
