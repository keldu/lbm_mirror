#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/particle/cell_list.hpp"
#include "../c++/particle/voxel_narrow.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;
constexpr uint64_t Cap = 16u;

template<uint64_t PC>
auto make_group(double radius, uint64_t res = 24u){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>> r;
	kel::lbm::data<sch::Scalar<sch::T>> d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{res});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y){
	auto& pos = g.template get<"particles">().at({i})
		.template get<"rigid_body">().template get<"position">();
	pos.at({{0u}}).set(x);
	pos.at({{1u}}).set(y);
}

SAW_TEST("Consistency / broad phase never rejects a real contact"){
	using namespace kel;

	/**
	 * The broad phase is allowed to over-report, never to under-report: a
	 * missed pair is a contact that silently never happens. Sweep a grid of
	 * separations and assert the implication in the direction that matters.
	 */
	auto g = make_group<2u>(3.0);
	auto& parts = g.template get<"particles">();

	for(int i = 0; i <= 60; ++i){
		double sep = 0.1 * i;
		place(g, 0u, 20.0, 20.0);
		place(g, 1u, 20.0 + sep, 20.0);

		bool broad = (lbm::broadphase_collision_check<sch::T,D>)(parts.at({0u}),parts.at({1u}));

		kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
		bool narrow = (lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
			parts.at({0u}), parts.at({1u}), m);

		SAW_EXPECT(!narrow || broad,
			std::string{"narrow found a contact the broad phase rejected at sep="}
			+ std::to_string(sep));
	}
}

SAW_TEST("Consistency / cell list finds the same contacts as brute force"){
	using namespace kel;

	/**
	 * The cell list is only an accelerator. If it ever disagrees with the
	 * exhaustive pair loop about WHICH pairs touch, every result built on it
	 * is suspect -- and the disagreement would be invisible in output.
	 */
	const double radius = 3.0;
	auto g = make_group<6u>(radius);
	auto& parts = g.template get<"particles">();

	// A cluster plus a couple of outliers, so some pairs touch and some do not.
	place(g, 0u, 20.0, 20.0);
	place(g, 1u, 24.5, 20.0);
	place(g, 2u, 22.0, 24.0);
	place(g, 3u, 21.0, 16.5);
	place(g, 4u, 50.0, 40.0);
	place(g, 5u, 54.0, 40.0);

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	auto cl = (lbm::make_cell_list<sch::T,D,Cap>)(extent, radius);
	(lbm::build_cell_list<sch::T,D,Cap>)(parts, cl);

	for(uint64_t i = 0u; i < 6u; ++i){
		// Brute force: everyone.
		bool brute[6] = {false,false,false,false,false,false};
		for(uint64_t j = 0u; j < 6u; ++j){
			if(j == i){ continue; }
			brute[j] = (lbm::broadphase_collision_check<sch::T,D>)(parts.at({i}),parts.at({j}));
		}

		// Via the bins.
		bool binned[6] = {false,false,false,false,false,false};
		kel::lbm::data<sch::FixedArray<sch::UInt64,D>> cell;
		auto& pos = parts.at({i}).template get<"rigid_body">().template get<"position">();
		for(uint64_t k = 0u; k < D; ++k){
			cell.at({k}).set(static_cast<uint64_t>(pos.at({{k}}).get()));
		}
		(lbm::for_each_bin_candidate<sch::T,D,Cap>)(cl, cell, [&](uint64_t j){
			if(j == i){ return; }
			if((lbm::broadphase_collision_check<sch::T,D>)(parts.at({i}),parts.at({j}))){
				binned[j] = true;
			}
		});

		for(uint64_t j = 0u; j < 6u; ++j){
			SAW_EXPECT(brute[j] == binned[j],
				std::string{"pair ("} + std::to_string(i) + "," + std::to_string(j)
				+ ") disagrees: brute=" + std::to_string(brute[j])
				+ " binned=" + std::to_string(binned[j]));
		}
	}
}

SAW_TEST("Consistency / contact accumulation is deterministic"){
	using namespace kel;

	/**
	 * Same input twice must give bitwise identical output. Anything else
	 * means a result that cannot be reproduced or bisected.
	 */
	lbm::dem_params<sch::T> dem{7.0, 0.35, 3.0, 0.6};

	auto run = [&](){
		auto g = make_group<4u>(3.0);
		auto& parts = g.template get<"particles">();
		place(g, 0u, 20.0, 20.0);
		place(g, 1u, 24.5, 20.3);
		place(g, 2u, 22.0, 24.0);
		place(g, 3u, 18.0, 23.0);

		for(uint64_t i = 0u; i < 4u; ++i){
			(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
		}
		for(uint64_t i = 0u; i < 4u; ++i){
			(lbm::accumulate_particle_contacts<sch::T,D>)(parts, i, dem, {1u});
		}

		std::array<double,8> out{};
		for(uint64_t i = 0u; i < 4u; ++i){
			auto& f = parts.at({i}).template get<"rigid_body">().template get<"force">();
			out[i*2u]      = f.at({{0u}}).get();
			out[i*2u + 1u] = f.at({{1u}}).get();
		}
		return out;
	};

	auto a = run();
	auto b = run();

	for(uint64_t i = 0u; i < 8u; ++i){
		SAW_EXPECT(a[i] == b[i],
			std::string{"run to run difference at slot "} + std::to_string(i));
	}
}

SAW_TEST("Consistency / voxel narrow phase handles a cube mask"){
	using namespace kel;

	/**
	 * The point of the voxel narrow phase is that it is shape agnostic. A
	 * cube is the cheapest shape a sphere test cannot stand in for: its
	 * corner is further out than its face, so a face-on contact and a
	 * corner-on contact at the same centre distance differ.
	 */
	auto g = make_group<2u>(5.0, 32u);
	auto& mask = g.template get<"mask">();

	// Overwrite the sphere mask with a cube of the same bounding radius.
	auto com = (lbm::fill_mask_from_sdf<sch::T,D>)(
		mask, 32u,
		[](const kel::lbm::data<sch::Vector<sch::T,D>>& u){
			return (lbm::sdf::cube<sch::T,D>)(u);
		}
	);
	g.template get<"center_of_mass">().at({0u}) = com;

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	auto& parts = g.template get<"particles">();

	// Face to face along x, overlapping.
	place(g, 0u, 28.0, 32.0);
	place(g, 1u, 36.0, 32.0);
	kel::lbm::data<sch::ContactManifold<sch::T,D>> m_face;
	bool face = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m_face);

	SAW_EXPECT(face, "overlapping cubes must register a contact");
	// A flat face gives a clean axis-aligned normal, which is exactly what a
	// centre-to-centre normal would also give here -- so check the shape
	// really is a cube by its reach instead.
	SAW_EXPECT(std::abs(m_face.template get<"normal">().at({{0u}}).get()) > 0.9,
		"face contact normal should be close to the x axis");

	// Diagonal placement: a sphere of the same bounding radius would already
	// have separated at this distance, the cube corners still meet.
	place(g, 0u, 28.0, 28.0);
	place(g, 1u, 35.0, 35.0);
	kel::lbm::data<sch::ContactManifold<sch::T,D>> m_corner;
	bool corner = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m_corner);

	SAW_EXPECT(corner,
		"cube corners should still touch where spheres would not");
}

SAW_TEST("Consistency / voxel and analytic agree on a sphere across offsets"){
	using namespace kel;

	/**
	 * Spot checks pass easily; sweep the configuration instead. Both routes
	 * describe the same body, so they must track each other everywhere, not
	 * just at one convenient separation.
	 */
	const double radius = 6.0;
	auto g = make_group<2u>(radius, 32u);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{72u,72u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	uint64_t compared = 0u;
	for(int dx = 2; dx <= 11; ++dx){
		for(int dy = 0; dy <= 4; ++dy){
			place(g, 0u, 36.0, 36.0);
			place(g, 1u, 36.0 + dx, 36.0 + dy);

			kel::lbm::data<sch::ContactManifold<sch::T,D>> mv, ma;
			bool hv = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,mv);
			bool ha = (lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
				parts.at({0u}), parts.at({1u}), ma);

			if(!ha){
				continue;
			}
			SAW_EXPECT(hv,
				std::string{"voxel missed a contact the analytic found at dx="}
				+ std::to_string(dx) + " dy=" + std::to_string(dy));

			auto nv = mv.template get<"normal">();
			auto na = ma.template get<"normal">();
			auto dot = nv.at({{0u}}).get()*na.at({{0u}}).get()
				+ nv.at({{1u}}).get()*na.at({{1u}}).get();

			SAW_EXPECT(dot > 0.9,
				std::string{"normals diverged at dx="} + std::to_string(dx)
				+ " dy=" + std::to_string(dy) + " dot=" + std::to_string(dot));

			auto diff = std::abs(mv.template get<"depth">().at({}).get()
				- ma.template get<"depth">().at({}).get());
			SAW_EXPECT(diff < 2.0,
				std::string{"depths diverged at dx="} + std::to_string(dx)
				+ " dy=" + std::to_string(dy) + " by " + std::to_string(diff));

			++compared;
		}
	}

	SAW_EXPECT(compared > 10u,
		std::string{"expected a meaningful number of overlapping cases, got "}
		+ std::to_string(compared));
}
}
