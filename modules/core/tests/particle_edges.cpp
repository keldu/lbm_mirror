#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/particle/cell_list.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

template<uint64_t PC>
auto make_group(double radius){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>> r;
	kel::lbm::data<sch::Scalar<sch::T>> d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{8u});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y){
	auto& pos = g.template get<"particles">().at({i})
		.template get<"rigid_body">().template get<"position">();
	pos.at({{0u}}).set(x);
	pos.at({{1u}}).set(y);
}

SAW_TEST("Edges / AABB clamps at the low wall too"){
	using namespace kel;

	/**
	 * The high side needed an explicit clamp against meta. The low side is
	 * handled by a ternary on the radius, which is easy to get backwards --
	 * and an index that underflows to a huge unsigned would sweep the whole
	 * field rather than failing visibly.
	 */
	using PG = lbm::sch::ParticleGroup<sch::T,D,2u,lbm::coll::Spheroid<sch::T>>;
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> meta{{64u,32u}};

	auto g = make_group<2u>(4.0);
	place(g, 0u, 1.0, 0.5);    // well inside both low walls
	place(g, 1u, 0.0, 0.0);    // exactly at the origin

	for(uint64_t i = 0u; i < 2u; ++i){
		kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> idx;
		idx.at({{0u}}).set(i);

		auto eo = lbm::particle_aabb<PG>::calculate(g, idx, meta);
		SAW_EXPECT(eo.is_value(), "aabb must succeed near the origin");

		auto& a = eo.get_value().template get<"a">();
		auto& b = eo.get_value().template get<"b">();

		for(uint64_t k = 0u; k < D; ++k){
			// Unsigned underflow would show up as an absurd lower bound.
			SAW_EXPECT(a.at({k}).get() <= meta.at({k}).get(),
				std::string{"lower bound underflowed for particle "} + std::to_string(i));
			SAW_EXPECT(b.at({k}).get() <= meta.at({k}).get(),
				std::string{"upper bound not clamped for particle "} + std::to_string(i));
			SAW_EXPECT(a.at({k}).get() < b.at({k}).get(),
				"box must be non-empty");
		}
	}
}

SAW_TEST("Edges / AABB of a particle larger than the domain"){
	using namespace kel;

	// Radius exceeds the field, so both bounds want to run off the ends.
	using PG = lbm::sch::ParticleGroup<sch::T,D,1u,lbm::coll::Spheroid<sch::T>>;
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> meta{{8u,8u}};

	auto g = make_group<1u>(20.0);
	place(g, 0u, 4.0, 4.0);

	auto eo = lbm::particle_aabb<PG>::calculate(g, {{0u}}, meta);
	SAW_EXPECT(eo.is_value(), "oversized particle must still produce a box");

	auto& a = eo.get_value().template get<"a">();
	auto& b = eo.get_value().template get<"b">();
	for(uint64_t k = 0u; k < D; ++k){
		SAW_EXPECT(a.at({k}).get() == 0u, "should start at the field edge");
		SAW_EXPECT(b.at({k}).get() == meta.at({k}).get(), "and stop at the far edge");
	}
}

SAW_TEST("Edges / cell list tolerates particles outside the domain"){
	using namespace kel;

	/**
	 * A particle that has drifted out of bounds must still bin somewhere
	 * rather than indexing past the end of the grid.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{32u,32u}};
	auto cl = (lbm::make_cell_list<sch::T,D,8u>)(extent, 2.0);

	auto g = make_group<3u>(2.0);
	place(g, 0u, -50.0, -50.0);    // far negative
	place(g, 1u, 500.0, 500.0);    // far positive
	place(g, 2u,  16.0,  16.0);    // legitimately inside

	auto& parts = g.template get<"particles">();
	(lbm::build_cell_list<sch::T,D,8u>)(parts, cl);

	SAW_EXPECT(cl.template get<"overflow">().at({0u}).get() == 0u,
		"clamped out-of-range particles should not overflow a bin");

	// The interior particle is still findable where it actually is.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> cell{{16u,16u}};
	bool found = false;
	(lbm::for_each_bin_candidate<sch::T,D,8u>)(cl, cell, [&](uint64_t p){
		if(p == 2u){ found = true; }
	});
	SAW_EXPECT(found, "the in-bounds particle must still be found");
}

SAW_TEST("Edges / cell list rebuild clears the previous frame"){
	using namespace kel;

	/**
	 * Bins are not self-clearing. If a rebuild forgot to reset the counts,
	 * stale entries would accumulate and particles would appear to be in two
	 * places at once -- for a few frames, then overflow.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{32u,32u}};
	auto cl = (lbm::make_cell_list<sch::T,D,8u>)(extent, 2.0);

	auto g = make_group<1u>(2.0);
	auto& parts = g.template get<"particles">();

	place(g, 0u, 5.0, 5.0);
	(lbm::build_cell_list<sch::T,D,8u>)(parts, cl);

	// Move it well away and rebuild.
	place(g, 0u, 25.0, 25.0);
	(lbm::build_cell_list<sch::T,D,8u>)(parts, cl);

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> old_cell{{5u,5u}};
	uint64_t stale = 0u;
	(lbm::for_each_bin_candidate<sch::T,D,8u>)(cl, old_cell, [&](uint64_t){ ++stale; });
	SAW_EXPECT(stale == 0u,
		std::string{"old position still reports "} + std::to_string(stale) + " entries");

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> new_cell{{25u,25u}};
	uint64_t fresh = 0u;
	(lbm::for_each_bin_candidate<sch::T,D,8u>)(cl, new_cell, [&](uint64_t){ ++fresh; });
	SAW_EXPECT(fresh == 1u, "new position must report exactly one entry");
}

SAW_TEST("Edges / zero relative velocity contact is pure spring"){
	using namespace kel;

	/**
	 * With both bodies at rest the dashpot contributes nothing, so the force
	 * must be exactly k*delta. Any velocity-convention slip shows up as a
	 * spurious damping term here.
	 */
	auto g = make_group<2u>(2.0);
	auto& parts = g.template get<"particles">();

	// At rest: position_old equals position.
	for(uint64_t i = 0u; i < 2u; ++i){
		auto& rb = parts.at({i}).template get<"rigid_body">();
		auto& pos = rb.template get<"position">();
		auto& old = rb.template get<"position_old">();
		for(uint64_t k = 0u; k < D; ++k){
			old.at({{k}}).set(pos.at({{k}}).get());
		}
	}
	place(g, 0u, 0.0, 0.0);
	place(g, 1u, 3.0, 0.0);
	for(uint64_t i = 0u; i < 2u; ++i){
		auto& rb = parts.at({i}).template get<"rigid_body">();
		auto& pos = rb.template get<"position">();
		auto& old = rb.template get<"position_old">();
		for(uint64_t k = 0u; k < D; ++k){
			old.at({{k}}).set(pos.at({{k}}).get());
		}
	}

	const double k_n = 3.0;
	lbm::dem_params<sch::T> dem{k_n, 0.9, 1.0, 0.5};

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
	(void)(lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);

	auto res = (lbm::particle_contact_response<sch::T,D>)(
		m, parts.at({0u}), parts.at({1u}), true, dem, {1u});

	// Overlap is 2+2-3 = 1, so the spring alone gives k*1 pointing -x.
	SAW_EXPECT(std::abs(res.force.at({{0u}}).get() + k_n) < 1e-12,
		std::string{"expected exactly -k*delta: "} + std::to_string(res.force.at({{0u}}).get()));
	SAW_EXPECT(res.force.at({{1u}}).get() == 0.0,
		"no tangential force without relative motion");
}

SAW_TEST("Edges / single particle group has no self contact"){
	using namespace kel;

	// The pair loop must skip j == i; a self contact would read as a huge
	// permanent overlap.
	auto g = make_group<1u>(2.0);
	auto& parts = g.template get<"particles">();
	place(g, 0u, 10.0, 10.0);

	lbm::dem_params<sch::T> dem{5.0, 0.3, 0.0, 0.0};

	(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
	(lbm::accumulate_particle_contacts<sch::T,D>)(parts, 0u, dem, {1u});

	for(uint64_t k = 0u; k < D; ++k){
		SAW_EXPECT(parts.at({0u}).template get<"rigid_body">()
			.template get<"force">().at({{k}}).get() == 0.0,
			"a lone particle must feel no contact force");
	}
}
}
