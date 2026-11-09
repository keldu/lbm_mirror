#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"
#include "../c++/particle/contact.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

template<uint64_t PC>
auto make_group(double radius, double density){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(density);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{8u});
}

template<typename G>
void set_state(G& g, uint64_t i, double x, double y, double vx, double vy){
	auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(x);
	rb.template get<"position">().at({{1u}}).set(y);
	// Position-Verlet carries velocity implicitly in the position pair.
	rb.template get<"position_old">().at({{0u}}).set(x - vx);
	rb.template get<"position_old">().at({{1u}}).set(y - vy);
}

template<typename G>
double vel_x(G& g, uint64_t i){
	auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();
	return rb.template get<"position">().at({{0u}}).get()
		- rb.template get<"position_old">().at({{0u}}).get();
}

/**
 * One full step of the pipeline for every particle: clear, accumulate
 * contacts, integrate.
 */
template<typename G>
void step_all(G& g, uint64_t n, const kel::lbm::dem_params<sch::T>& dem){
	using namespace kel;
	auto& parts = g.template get<"particles">();
	for(uint64_t i = 0u; i < n; ++i){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
	}
	for(uint64_t i = 0u; i < n; ++i){
		(lbm::accumulate_particle_contacts<sch::T,D>)(parts, i, dem, {1u});
	}
	for(uint64_t i = 0u; i < n; ++i){
		(lbm::integrate_particle<sch::T,D>)(parts.at({i}), {}, {1u});
	}
}

SAW_TEST("Dynamics / head-on collision reverses both bodies"){
	using namespace kel;

	auto g = make_group<2u>(2.0, 1.0);
	// Approaching, just touching.
	set_state(g, 0u, 0.0, 0.0,  0.05, 0.0);
	set_state(g, 1u, 4.0, 0.0, -0.05, 0.0);

	// Stiff enough to turn them around inside the window, lightly damped.
	lbm::dem_params<sch::T> dem{2.0, 0.1, 0.0, 0.0};

	bool reversed = false;
	for(uint64_t step = 0u; step < 400u; ++step){
		step_all(g, 2u, dem);
		if(vel_x(g,0u) < 0.0 && vel_x(g,1u) > 0.0){
			reversed = true;
			break;
		}
	}

	SAW_EXPECT(reversed, "an approaching pair must end up separating");
}

SAW_TEST("Dynamics / collision does not create energy"){
	using namespace kel;

	/**
	 * The dashpot can only remove energy. If the sign of the damping term or
	 * the velocity convention were wrong, a bounce would come back faster
	 * than it went in -- which is the classic way a DEM blows up slowly
	 * enough that you do not notice for a thousand steps.
	 */
	auto g = make_group<2u>(2.0, 1.0);
	const double v0 = 0.05;
	set_state(g, 0u, 0.0, 0.0,  v0, 0.0);
	set_state(g, 1u, 4.0, 0.0, -v0, 0.0);

	lbm::dem_params<sch::T> dem{2.0, 0.2, 0.0, 0.0};

	double peak = 0.0;
	for(uint64_t step = 0u; step < 600u; ++step){
		step_all(g, 2u, dem);

		auto s0 = std::abs(vel_x(g,0u));
		auto s1 = std::abs(vel_x(g,1u));
		if(s0 > peak){ peak = s0; }
		if(s1 > peak){ peak = s1; }
	}

	SAW_EXPECT(peak <= v0 * 1.02,
		std::string{"no body may exceed its incoming speed, peak was "}
		+ std::to_string(peak) + " against " + std::to_string(v0));
}

SAW_TEST("Dynamics / resting contact settles at the force balance"){
	using namespace kel;

	/**
	 * A body held against a wall by a constant body acceleration must come to
	 * rest where the spring balances the load, at an overlap of m*g/k.
	 *
	 * Note this needs confinement to mean anything: two FREE bodies pushed
	 * apart just separate and coast, so there is no equilibrium to find.
	 */
	auto g = make_group<1u>(2.0, 1.0);
	auto& parts = g.template get<"particles">();
	auto& rb = parts.at({0u}).template get<"rigid_body">();

	const double mass = rb.template get<"mass">().at({}).get();
	const double k_n = 1.0;
	const double gravity = 0.0005;

	// Start just touching the low x wall, at rest.
	set_state(g, 0u, 2.0, 16.0, 0.0, 0.0);

	lbm::dem_params<sch::T> dem{k_n, 0.8, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	// Gravity pointing at the wall, as a body acceleration.
	kel::lbm::data<sch::Vector<sch::T,D>> body_acc;
	body_acc.at({{0u}}).set(-gravity);
	body_acc.at({{1u}}).set(0.0);

	auto x_of = [&](){
		return rb.template get<"position">().at({{0u}}).get();
	};

	for(uint64_t step = 0u; step < 4000u; ++step){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
		(lbm::accumulate_box_wall_contacts<sch::T,D>)(parts.at({0u}), extent, dem, {1u});
		(lbm::integrate_particle<sch::T,D>)(parts.at({0u}), body_acc, {1u});
	}

	auto settled = x_of();
	auto expect_overlap = mass * gravity / k_n;
	auto got_overlap = 2.0 - settled;

	SAW_EXPECT(std::isfinite(settled), "must stay finite");
	SAW_EXPECT(std::abs(got_overlap - expect_overlap) < 0.1 * expect_overlap,
		std::string{"expected overlap m*g/k = "} + std::to_string(expect_overlap)
		+ " got " + std::to_string(got_overlap));

	// And it must actually be at rest, not still travelling.
	auto speed = std::abs(x_of() - rb.template get<"position_old">().at({{0u}}).get());
	SAW_EXPECT(speed < 1e-4,
		std::string{"should have come to rest, residual speed "} + std::to_string(speed));
}

SAW_TEST("Dynamics / three body momentum is conserved"){
	using namespace kel;

	/**
	 * Two bodies cancelling is weak evidence -- a sign error can cancel
	 * pairwise and still be wrong once a particle has two neighbours at once.
	 */
	auto g = make_group<3u>(2.0, 1.0);
	set_state(g, 0u, 0.0, 0.0,  0.03,  0.01);
	set_state(g, 1u, 3.5, 0.0, -0.01,  0.02);
	set_state(g, 2u, 7.0, 0.3,  0.00, -0.02);

	lbm::dem_params<sch::T> dem{5.0, 0.3, 2.0, 0.4};

	auto& parts = g.template get<"particles">();
	for(uint64_t i = 0u; i < 3u; ++i){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
	}
	for(uint64_t i = 0u; i < 3u; ++i){
		(lbm::accumulate_particle_contacts<sch::T,D>)(parts, i, dem, {1u});
	}

	// The middle body touches both neighbours, so this is the case that
	// pairwise cancellation alone would not catch.
	for(uint64_t k = 0u; k < D; ++k){
		double sum = 0.0;
		for(uint64_t i = 0u; i < 3u; ++i){
			sum += parts.at({i}).template get<"rigid_body">()
				.template get<"force">().at({{k}}).get();
		}
		SAW_EXPECT(std::abs(sum) < 1e-12,
			std::string{"net force in component "} + std::to_string(k)
			+ " should vanish: " + std::to_string(sum));
	}

	// And the middle body must actually be feeling both sides.
	auto f1 = parts.at({1u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get();
	SAW_EXPECT(std::abs(f1) > 0.0, "the middle body must be loaded from both sides");
}

SAW_TEST("Dynamics / wall stops an incoming body"){
	using namespace kel;

	auto g = make_group<1u>(2.0, 1.0);
	// Heading into the low x wall.
	set_state(g, 0u, 2.5, 16.0, -0.05, 0.0);

	lbm::dem_params<sch::T> dem{2.0, 0.2, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	auto& parts = g.template get<"particles">();
	double deepest = 2.5;

	for(uint64_t step = 0u; step < 400u; ++step){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
		(lbm::accumulate_box_wall_contacts<sch::T,D>)(parts.at({0u}), extent, dem, {1u});
		(lbm::integrate_particle<sch::T,D>)(parts.at({0u}), {}, {1u});

		auto x = parts.at({0u}).template get<"rigid_body">()
			.template get<"position">().at({{0u}}).get();
		if(x < deepest){ deepest = x; }
	}

	auto final_x = parts.at({0u}).template get<"rigid_body">()
		.template get<"position">().at({{0u}}).get();

	SAW_EXPECT(deepest > 0.0,
		std::string{"the body must not pass through the wall, reached "}
		+ std::to_string(deepest));
	SAW_EXPECT(final_x > deepest,
		"and it must be pushed back out again");
}

SAW_TEST("Dynamics / sub steps do not change the trajectory"){
	using namespace kel;

	/**
	 * sub_steps subdivides one unit of time, so the displacement under a
	 * constant acceleration should come out the same regardless of how many
	 * pieces it is cut into. A mis-scaled dt would show up here immediately.
	 */
	auto run = [&](uint64_t steps){
		auto g = make_group<1u>(2.0, 1.0);
		auto& p = g.template get<"particles">().at({0u});
		auto& rb = p.template get<"rigid_body">();
		rb.template get<"force">().at({{0u}}).set(rb.template get<"mass">().at({}).get());
		(lbm::integrate_particle<sch::T,D>)(p, {}, {steps});
		return rb.template get<"position">().at({{0u}}).get();
	};

	auto one = run(1u);
	auto four = run(4u);
	auto sixteen = run(16u);

	SAW_EXPECT(one > 0.0, "must move at all");
	// Verlet from rest: total displacement converges as the step shrinks.
	SAW_EXPECT(std::abs(four - one) / one < 0.5,
		std::string{"1 vs 4 sub steps diverged: "} + std::to_string(one)
		+ " vs " + std::to_string(four));
	SAW_EXPECT(std::abs(sixteen - four) < std::abs(four - one),
		"refining the sub step should be converging, not drifting");
}
}
