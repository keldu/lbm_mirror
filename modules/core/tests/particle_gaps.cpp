#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/particle/mask.hpp"
#include "../c++/particle/voxel_narrow.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

template<uint64_t PC>
auto make_group(double radius, uint64_t res = 32u){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{res});
}

template<typename G>
void rest_at(G& g, uint64_t i, double x, double y){
	auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();
	const double v[2] = {x,y};
	for(uint64_t k = 0u; k < D; ++k){
		rb.template get<"position">().at({{k}}).set(v[k]);
		rb.template get<"position_old">().at({{k}}).set(v[k]);
	}
}

SAW_TEST("Gaps / mask sampling outside the box extrapolates monotonically"){
	using namespace kel;

	/**
	 * mask_sample_local clamps the lookup to the stored box and adds back the
	 * distance to the box. That extrapolation branch has no other coverage,
	 * and getting it wrong would make far-field queries read as *inside*.
	 */
	auto g = make_group<1u>(3.0);
	auto& mask = g.template get<"mask">();
	const uint64_t R = 32u;

	double prev = -2.0;
	for(int i = 0; i <= 12; ++i){
		double d = 0.5 * i;   // walk out along +x, past the box edge at 1.0

		kel::lbm::data<sch::Vector<sch::T,D>> u;
		u.at({{0u}}).set(d);
		u.at({{1u}}).set(0.0);

		auto s = (lbm::mask_sample_local<sch::T,D>)(mask,u,R);

		SAW_EXPECT(std::isfinite(s),
			std::string{"sample must stay finite at u="} + std::to_string(d));
		SAW_EXPECT(s > prev - 1e-9,
			std::string{"distance must grow outward, broke at u="} + std::to_string(d));
		if(d > 1.5){
			SAW_EXPECT(s > 0.0,
				std::string{"well outside must read positive, got "} + std::to_string(s)
				+ " at u=" + std::to_string(d));
		}
		prev = s;
	}
}

SAW_TEST("Gaps / solid fraction is exact at the shell edges"){
	using namespace kel;

	// The ramp endpoints are where an off-by-half would hide.
	const double eps = 2.0;
	SAW_EXPECT((lbm::solid_fraction_from_sdf<sch::T>)(-eps/2, eps) == 1.0,
		"the inner edge of the shell must be fully solid");
	SAW_EXPECT((lbm::solid_fraction_from_sdf<sch::T>)( eps/2, eps) == 0.0,
		"the outer edge must be fully fluid");

	// Midpoint of a cos^2 ramp is exactly one half.
	auto mid = (lbm::solid_fraction_from_sdf<sch::T>)(0.0, eps);
	SAW_EXPECT(std::abs(mid - 0.5) < 1e-12,
		std::string{"the surface itself should read 0.5: "} + std::to_string(mid));
}

SAW_TEST("Gaps / one shared mask serves particles of different sizes"){
	using namespace kel;

	/**
	 * This is the central claim of the normalized-mask design: the group holds
	 * one shape and each particle scales it by its own radius. If the scaling
	 * were dropped the two bodies below would read identically.
	 */
	auto g = make_group<2u>(4.0);
	auto& parts = g.template get<"particles">();
	auto& com = g.template get<"center_of_mass">().at({0u});
	const uint64_t R = 32u;

	rest_at(g, 0u, 20.0, 20.0);
	rest_at(g, 1u, 50.0, 20.0);

	// Shrink the second particle; same group, same mask.
	parts.at({1u}).template get<"collision">().template get<"radius">().at({}).set(1.0);

	// Two lattice units out from each centre: inside the big one, outside the
	// small one.
	kel::lbm::data<sch::Vector<sch::T,D>> probe_big;
	probe_big.at({{0u}}).set(22.0);
	probe_big.at({{1u}}).set(20.0);

	kel::lbm::data<sch::Vector<sch::T,D>> probe_small;
	probe_small.at({{0u}}).set(52.0);
	probe_small.at({{1u}}).set(20.0);

	auto d_big = (lbm::mask_signed_distance<sch::T,D>)(g,parts.at({0u}),probe_big,R);
	auto d_small = (lbm::mask_signed_distance<sch::T,D>)(g,parts.at({1u}),probe_small,R);

	SAW_EXPECT(d_big < -0.5,
		std::string{"2 units into a radius 4 body must be inside: "} + std::to_string(d_big));
	SAW_EXPECT(d_small > 0.5,
		std::string{"2 units from a radius 1 body must be outside: "} + std::to_string(d_small));

	// And the physical bounding radius really is the particle's radius, which
	// is what makes the broad phase a valid bound for the voxel narrow phase.
	kel::lbm::data<sch::Vector<sch::T,D>> at_edge;
	at_edge.at({{0u}}).set(51.0);
	at_edge.at({{1u}}).set(20.0);
	auto d_edge = (lbm::mask_signed_distance<sch::T,D>)(g,parts.at({1u}),at_edge,R);
	SAW_EXPECT(std::abs(d_edge) < 0.15,
		std::string{"one radius out should be the surface: "} + std::to_string(d_edge));
}

SAW_TEST("Gaps / mixed size pair contacts correctly"){
	using namespace kel;

	auto g = make_group<2u>(4.0);
	auto& parts = g.template get<"particles">();

	parts.at({1u}).template get<"collision">().template get<"radius">().at({}).set(1.0);

	// Centres 4.5 apart: radii 4 + 1 = 5, so they overlap by 0.5.
	rest_at(g, 0u, 20.0, 20.0);
	rest_at(g, 1u, 24.5, 20.0);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
	bool hit = (lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);

	SAW_EXPECT(hit, "a mixed size overlapping pair must register");
	SAW_EXPECT(std::abs(m.template get<"depth">().at({}).get() - 0.5) < 1e-12,
		std::string{"depth should be r0+r1-d = 0.5: "}
		+ std::to_string(m.template get<"depth">().at({}).get()));

	// The contact point must lie on the small body's surface, i.e. much
	// closer to it than to the large one.
	auto px = m.template get<"point">().at({{0u}}).get();
	SAW_EXPECT(px > 23.0,
		std::string{"contact should sit near the small body: "} + std::to_string(px));
}

SAW_TEST("Gaps / the high wall pushes inward"){
	using namespace kel;

	/**
	 * Only the low wall and the corner had coverage. The high face has its own
	 * sign and its own offset from the extent, either of which could be wrong
	 * independently.
	 */
	auto g = make_group<1u>(2.0);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};
	const double k_n = 4.0;
	lbm::dem_params<sch::T> dem{k_n, 0.0, 0.0, 0.0};

	// Overlapping the high x wall by 0.5.
	rest_at(g, 0u, 62.5, 16.0);
	(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
	(lbm::accumulate_box_wall_contacts<sch::T,D>)(parts.at({0u}), extent, dem, {1u});

	auto fx = parts.at({0u}).template get<"rigid_body">()
		.template get<"force">().at({{0u}}).get();
	SAW_EXPECT(std::abs(fx + k_n * 0.5) < 1e-12,
		std::string{"high wall should push -x by k*depth: "} + std::to_string(fx));

	// Same on the high y face, which uses the other extent.
	auto g2 = make_group<1u>(2.0);
	auto& p2 = g2.template get<"particles">();
	rest_at(g2, 0u, 32.0, 30.5);
	(lbm::reset_particle_accumulators<sch::T,D>)(p2.at({0u}));
	(lbm::accumulate_box_wall_contacts<sch::T,D>)(p2.at({0u}), extent, dem, {1u});

	auto fy = p2.at({0u}).template get<"rigid_body">()
		.template get<"force">().at({{1u}}).get();
	SAW_EXPECT(std::abs(fy + k_n * 0.5) < 1e-12,
		std::string{"high y wall should push -y by k*depth: "} + std::to_string(fy));
}

SAW_TEST("Gaps / friction is capped by Coulomb"){
	using namespace kel;

	/**
	 * The tangential force is min(gamma_t |v_t|, mu f_n). With a large
	 * gamma_t the cap must bind, otherwise friction can exceed the normal
	 * load and a sliding contact gains energy.
	 */
	auto g = make_group<2u>(2.0);
	auto& parts = g.template get<"particles">();

	// Overlapping, sliding hard past each other.
	auto set = [&](uint64_t i, double x, double y, double vy){
		auto& rb = parts.at({i}).template get<"rigid_body">();
		rb.template get<"position">().at({{0u}}).set(x);
		rb.template get<"position">().at({{1u}}).set(y);
		rb.template get<"position_old">().at({{0u}}).set(x);
		rb.template get<"position_old">().at({{1u}}).set(y - vy);
	};
	set(0u, 0.0, 0.0,  1.0);
	set(1u, 3.0, 0.0, -1.0);

	const double k_n = 2.0;
	const double mu = 0.25;
	// gamma_t absurdly large so the viscous term would dominate uncapped.
	lbm::dem_params<sch::T> dem{k_n, 0.0, 1000.0, mu};

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
	(void)(lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);

	auto res = (lbm::particle_contact_response<sch::T,D>)(
		m, parts.at({0u}), parts.at({1u}), true, dem, {1u});

	// Undamped normal force is k*delta = 2*1 = 2, so friction caps at 0.5.
	auto f_n = std::abs(res.force.at({{0u}}).get());
	auto f_t = std::abs(res.force.at({{1u}}).get());

	SAW_EXPECT(std::abs(f_n - k_n * 1.0) < 1e-12,
		std::string{"normal force should be k*delta: "} + std::to_string(f_n));
	SAW_EXPECT(std::abs(f_t - mu * f_n) < 1e-9,
		std::string{"friction should sit exactly on the Coulomb cap "}
		+ std::to_string(mu * f_n) + ", got " + std::to_string(f_t));
}

SAW_TEST("Gaps / stiffness helper matches its own definition"){
	using namespace kel;

	/**
	 * dem_stiffness_for is the only guidance a caller gets for choosing k_n,
	 * so it should at least be self consistent: a body hitting at v0 with the
	 * suggested stiffness should stop near delta_max.
	 */
	const double m_eff = 4.0;
	const double v0 = 0.1;
	const double delta_max = 0.05;

	auto k = (lbm::dem_stiffness_for<sch::T>)(m_eff, v0, delta_max);

	// Energy balance: k delta^2 / 2 == m v^2 / 2.
	auto spring_energy = 0.5 * k * delta_max * delta_max;
	auto kinetic = 0.5 * m_eff * v0 * v0;

	SAW_EXPECT(std::abs(spring_energy - kinetic) < 1e-12,
		std::string{"energies should match: "} + std::to_string(spring_energy)
		+ " vs " + std::to_string(kinetic));
	SAW_EXPECT(k > 0.0, "stiffness must be positive");
}

SAW_TEST("Gaps / voxel contact is deterministic"){
	using namespace kel;

	// The overlap integral walks the lattice; the sweep order must not drift
	// between calls or results stop being reproducible.
	auto g = make_group<2u>(5.0);
	auto& parts = g.template get<"particles">();
	rest_at(g, 0u, 28.0, 30.0);
	rest_at(g, 1u, 35.0, 32.0);

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> a, b;
	bool ha = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,a);
	bool hb = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,b);

	SAW_EXPECT(ha && hb, "both calls must find the contact");
	SAW_EXPECT(a.template get<"depth">().at({}).get() == b.template get<"depth">().at({}).get(),
		"depth must be bitwise reproducible");
	for(uint64_t k = 0u; k < D; ++k){
		SAW_EXPECT(a.template get<"normal">().at({{k}}).get()
				== b.template get<"normal">().at({{k}}).get(),
			"normal must be bitwise reproducible");
	}
}
}
