#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/particle/mask.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

template<uint64_t PC>
auto make_group(double radius, double density){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>> r;
	kel::lbm::data<sch::Scalar<sch::T>> d;
	r.at({}).set(radius);
	d.at({}).set(density);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{16u});
}

SAW_TEST("Rotation / support flag matches the integrator"){
	using namespace kel;

	/**
	 * 3D rotation is deliberately absent rather than approximated: a scalar
	 * inertia only describes an isotropic body, and integrating the rotation
	 * vector as a displacement cannot represent non-commuting composition.
	 * The flag exists so torque accumulation and angular integration cannot
	 * drift apart -- accumulating a torque nothing consumes would be a silent
	 * no-op.
	 */
	static_assert(lbm::particle_angular_supported<2u>, "2D rotation must be live");
	static_assert(!lbm::particle_angular_supported<3u>, "3D rotation must stay gated");
}

SAW_TEST("Rotation / constant torque gives constant angular acceleration"){
	using namespace kel;

	/**
	 * Nothing in the library ever wrote angular_acceleration before contact
	 * started producing torque, so verlet_step_lambda's angular half had
	 * never actually run. This is the first exercise of that path.
	 */
	auto g = make_group<1u>(2.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();

	auto inertia = rb.template get<"inertia">().at({}).get();
	SAW_EXPECT(inertia > 0.0,
		std::string{"inertia must be positive: "} + std::to_string(inertia));

	// Pure torque, no force.
	const double torque = 3.0;
	rb.template get<"torque">().at({}).set(torque);

	(lbm::integrate_particle<sch::T,D>)(p, {}, {1u});

	auto ang_acc = rb.template get<"angular_acceleration">().at({}).get();
	SAW_EXPECT(std::abs(ang_acc - torque / inertia) < 1e-12,
		std::string{"angular acceleration should be torque/inertia: "}
		+ std::to_string(ang_acc));

	// One unit step of theta = 2 theta - theta_old + alpha dt^2, from rest.
	auto rot = rb.template get<"rotation">().at({}).get();
	SAW_EXPECT(std::abs(rot - ang_acc) < 1e-12,
		std::string{"rotation after one unit step should equal alpha: "}
		+ std::to_string(rot));
}

SAW_TEST("Rotation / spins up under sustained torque"){
	using namespace kel;

	auto g = make_group<1u>(2.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();

	auto inertia = rb.template get<"inertia">().at({}).get();
	const double torque = 1.0;
	const double alpha = torque / inertia;

	// Hold the torque for 10 unit steps; position-Verlet from rest should
	// track theta = alpha t^2 / 2.
	for(uint64_t step = 0u; step < 10u; ++step){
		rb.template get<"torque">().at({}).set(torque);
		(lbm::integrate_particle<sch::T,D>)(p, {}, {1u});
	}

	auto rot = rb.template get<"rotation">().at({}).get();
	auto expect = 0.5 * alpha * 10.0 * 10.0;

	// Verlet from rest lags the closed form by about half a step of drift.
	SAW_EXPECT(rot > 0.0, "must have spun up");
	SAW_EXPECT(std::abs(rot - expect) / expect < 0.15,
		std::string{"expected about "} + std::to_string(expect)
		+ " got " + std::to_string(rot));
}

SAW_TEST("Rotation / no torque means no spin"){
	using namespace kel;

	auto g = make_group<1u>(2.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();

	// Linear force only.
	rb.template get<"force">().at({{0u}}).set(5.0);
	(lbm::reset_particle_accumulators<sch::T,D>)(p);
	rb.template get<"force">().at({{0u}}).set(5.0);

	(lbm::integrate_particle<sch::T,D>)(p, {}, {4u});

	SAW_EXPECT(rb.template get<"rotation">().at({}).get() == 0.0,
		"a pure force must not induce spin");
	SAW_EXPECT(rb.template get<"position">().at({{0u}}).get() > 0.0,
		"but it must move the body");
}

SAW_TEST("Rotation / tangential contact produces torque"){
	using namespace kel;

	/**
	 * A glancing contact with friction must spin the bodies, and the two
	 * halves must be consistent: equal and opposite forces, with each torque
	 * taken about its own centre.
	 */
	auto g = make_group<2u>(2.0, 1.0);
	auto& parts = g.template get<"particles">();

	auto place = [&](uint64_t i, double x, double y, double ox, double oy){
		auto& rb = parts.at({i}).template get<"rigid_body">();
		rb.template get<"position">().at({{0u}}).set(x);
		rb.template get<"position">().at({{1u}}).set(y);
		rb.template get<"position_old">().at({{0u}}).set(x - ox);
		rb.template get<"position_old">().at({{1u}}).set(y - oy);
	};

	// Overlapping, sliding past each other along y.
	place(0u, 0.0, 0.0,  0.0,  0.2);
	place(1u, 3.0, 0.0,  0.0, -0.2);

	lbm::dem_params<sch::T> dem{10.0, 0.2, 5.0, 0.8};

	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
	}
	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::accumulate_particle_contacts<sch::T,D>)(g.template get<"particles">(), i, dem, {1u});
	}

	auto t0 = parts.at({0u}).template get<"rigid_body">().template get<"torque">().at({}).get();
	auto t1 = parts.at({1u}).template get<"rigid_body">().template get<"torque">().at({}).get();

	SAW_EXPECT(std::abs(t0) > 0.0, "sliding contact must produce torque");

	// Equal and opposite forces about mirrored lever arms means same-sign
	// torque: both bodies are spun the same way by the shear between them.
	SAW_EXPECT(t0 * t1 > 0.0,
		std::string{"both bodies should spin the same way: "}
		+ std::to_string(t0) + " and " + std::to_string(t1));

	// Linear halves still cancel exactly.
	for(uint64_t k = 0u; k < D; ++k){
		auto f0 = parts.at({0u}).template get<"rigid_body">().template get<"force">().at({{k}}).get();
		auto f1 = parts.at({1u}).template get<"rigid_body">().template get<"force">().at({{k}}).get();
		SAW_EXPECT(f0 == -f1, "linear momentum must still be conserved exactly");
	}
}

SAW_TEST("Rotation / mask actually turns with the body"){
	using namespace kel;

	/**
	 * mask_local_to_global applies Rot(theta). Until torque existed, theta
	 * never changed, so this had no observable effect.
	 */
	auto g = make_group<1u>(3.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(20.0);
	rb.template get<"position">().at({{1u}}).set(20.0);

	auto& com = g.template get<"center_of_mass">().at({0u});

	// A point on the local +x axis.
	kel::lbm::data<sch::Vector<sch::T,D>> u;
	u.at({{0u}}).set(1.0);
	u.at({{1u}}).set(0.0);

	rb.template get<"rotation">().at({}).set(0.0);
	auto x0 = (lbm::mask_local_to_global<sch::T,D>)(u,p,com);

	// Quarter turn: +x should swing to +y.
	rb.template get<"rotation">().at({}).set(3.14159265358979323846 / 2.0);
	auto x90 = (lbm::mask_local_to_global<sch::T,D>)(u,p,com);

	SAW_EXPECT(std::abs(x0.at({{0u}}).get() - 23.0) < 0.05,
		std::string{"unrotated should sit at +radius in x: "}
		+ std::to_string(x0.at({{0u}}).get()));
	SAW_EXPECT(std::abs(x90.at({{1u}}).get() - 23.0) < 0.05,
		std::string{"quarter turn should sit at +radius in y: "}
		+ std::to_string(x90.at({{1u}}).get()));
	SAW_EXPECT(std::abs(x90.at({{0u}}).get() - 20.0) < 0.05,
		"and back on the centre line in x");
}
}
