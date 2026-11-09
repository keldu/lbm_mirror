#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/cell_list.hpp"
#include "../c++/particle/couple.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;
constexpr uint64_t Cap = 8u;

template<uint64_t PC>
auto make_group(double radius){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{4u});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y){
	auto& pos = g.template get<"particles">().at({i})
		.template get<"rigid_body">().template get<"position">();
	pos.at({{0u}}).set(x);
	pos.at({{1u}}).set(y);
}

SAW_TEST("CellList / bins and finds particles"){
	using namespace kel;

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};
	auto cl = (lbm::make_cell_list<sch::T,D,Cap>)(extent, 2.0);

	auto g = make_group<3u>(2.0);
	place(g, 0u,  5.0,  5.0);
	place(g, 1u,  6.0,  5.0);   // same neighbourhood as 0
	place(g, 2u, 50.0, 25.0);   // far away

	auto& parts = g.template get<"particles">();
	(lbm::build_cell_list<sch::T,D,Cap>)(parts, cl);

	SAW_EXPECT(cl.template get<"overflow">().at({0u}).get() == 0u, "no overflow expected");

	// A cell next to particles 0 and 1 must see both, and not particle 2.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> near{{5u,5u}};
	bool saw[3] = {false,false,false};
	(lbm::for_each_bin_candidate<sch::T,D,Cap>)(cl, near, [&](uint64_t p){ saw[p] = true; });

	SAW_EXPECT(saw[0], "particle 0 must be a candidate");
	SAW_EXPECT(saw[1], "particle 1 must be a candidate");
	SAW_EXPECT(!saw[2], "a distant particle must be pruned");

	// And the far cell sees only particle 2.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> far{{50u,25u}};
	bool saw2[3] = {false,false,false};
	(lbm::for_each_bin_candidate<sch::T,D,Cap>)(cl, far, [&](uint64_t p){ saw2[p] = true; });
	SAW_EXPECT(saw2[2], "particle 2 must be a candidate at its own cell");
	SAW_EXPECT(!saw2[0] && !saw2[1], "distant particles must be pruned");
}

SAW_TEST("CellList / never misses a covering particle"){
	using namespace kel;

	/**
	 * The bin size rule is load bearing: bins are one diameter wide so a
	 * covering particle is always within the 3^D neighbourhood. Check it
	 * exhaustively against brute force rather than trusting the argument.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{48u,24u}};
	const double radius = 3.0;
	auto cl = (lbm::make_cell_list<sch::T,D,Cap>)(extent, radius);

	auto g = make_group<4u>(radius);
	place(g, 0u,  4.0,  4.0);
	place(g, 1u, 20.0, 12.0);
	place(g, 2u, 21.5, 13.0);
	place(g, 3u, 44.0, 20.0);

	auto& parts = g.template get<"particles">();
	(lbm::build_cell_list<sch::T,D,Cap>)(parts, cl);

	uint64_t checked = 0u;
	lbm::iterator<D>::apply([&](const auto& cell){
		++checked;

		bool candidate[4] = {false,false,false,false};
		(lbm::for_each_bin_candidate<sch::T,D,Cap>)(cl, cell, [&](uint64_t p){ candidate[p] = true; });

		for(uint64_t p = 0u; p < 4u; ++p){
			auto& pos = parts.at({p}).template get<"rigid_body">().template get<"position">();
			double dx = static_cast<double>(cell.at({0u}).get()) - pos.at({{0u}}).get();
			double dy = static_cast<double>(cell.at({1u}).get()) - pos.at({{1u}}).get();
			bool covers = (dx*dx + dy*dy) <= radius*radius;

			// Over-reporting is fine; missing a cover is not.
			SAW_EXPECT(!covers || candidate[p],
				std::string{"bin query missed particle "} + std::to_string(p));
		}
	},{},extent);

	SAW_EXPECT(checked == 48u*24u, "expected to sweep the whole lattice");
}

SAW_TEST("CellList / overflow is reported not dropped"){
	using namespace kel;

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{16u,16u}};
	auto cl = (lbm::make_cell_list<sch::T,D,2u>)(extent, 2.0);

	// Four particles stacked in one bin, capacity two.
	auto g = make_group<4u>(2.0);
	for(uint64_t i = 0u; i < 4u; ++i){
		place(g, i, 5.0, 5.0);
	}

	auto& parts = g.template get<"particles">();
	(lbm::build_cell_list<sch::T,D,2u>)(parts, cl);

	SAW_EXPECT(cl.template get<"overflow">().at({0u}).get() == 2u,
		std::string{"expected 2 overflows, got "}
		+ std::to_string(cl.template get<"overflow">().at({0u}).get()));
}

SAW_TEST("Couple / overlapping particles compose instead of clobbering"){
	using namespace kel;

	/**
	 * The per-particle stamp assigned porosity, so for two overlapping
	 * particles whichever work item ran last simply won. The per-cell gather
	 * multiplies the fluid fractions instead, which is the whole point of
	 * inverting the loop.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{32u,32u}};
	auto cl = (lbm::make_cell_list<sch::T,D,Cap>)(extent, 3.0);

	auto g = make_group<2u>(3.0);
	place(g, 0u, 15.0, 16.0);
	place(g, 1u, 17.0, 16.0);   // overlaps particle 0

	auto& parts = g.template get<"particles">();
	(lbm::build_cell_list<sch::T,D,Cap>)(parts, cl);

	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.5);

	// Deep inside both bodies: fully solid.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> mid{{16u,16u}};
	auto p_mid = (lbm::cell_porosity<sch::T,D,Cap>)(parts,cl,mid,eps);
	SAW_EXPECT(p_mid.at({}).get() == 0.0,
		std::string{"overlap region must read fully solid: "}
		+ std::to_string(p_mid.at({}).get()));

	// Well outside both: fully fluid.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> out{{2u,2u}};
	auto p_out = (lbm::cell_porosity<sch::T,D,Cap>)(parts,cl,out,eps);
	SAW_EXPECT(p_out.at({}).get() == 1.0,
		std::string{"clear fluid must read 1: "} + std::to_string(p_out.at({}).get()));
}

SAW_TEST("Couple / is order independent"){
	using namespace kel;

	/**
	 * The property the per-particle scatter could not provide: the result
	 * must not depend on which particle was processed first.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{32u,32u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.5);

	auto a = make_group<2u>(3.0);
	place(a, 0u, 15.0, 16.0);
	place(a, 1u, 17.0, 16.0);

	auto b = make_group<2u>(3.0);
	place(b, 0u, 17.0, 16.0);   // same pair, swapped order
	place(b, 1u, 15.0, 16.0);

	auto cl_a = (lbm::make_cell_list<sch::T,D,Cap>)(extent, 3.0);
	auto cl_b = (lbm::make_cell_list<sch::T,D,Cap>)(extent, 3.0);
	(lbm::build_cell_list<sch::T,D,Cap>)(a.template get<"particles">(), cl_a);
	(lbm::build_cell_list<sch::T,D,Cap>)(b.template get<"particles">(), cl_b);

	lbm::iterator<D>::apply([&](const auto& cell){
		auto pa = (lbm::cell_porosity<sch::T,D,Cap>)(a.template get<"particles">(),cl_a,cell,eps);
		auto pb = (lbm::cell_porosity<sch::T,D,Cap>)(b.template get<"particles">(),cl_b,cell,eps);
		SAW_EXPECT(pa.at({}).get() == pb.at({}).get(),
			"porosity must not depend on particle ordering");
	},{{10u,10u}},{{24u,24u}});
}
}
