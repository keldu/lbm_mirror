#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/voxel_narrow.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

auto make_pair(double radius, uint64_t res = 32u){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,2u>(r,d,{res});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y){
	auto& pos = g.template get<"particles">().at({i})
		.template get<"rigid_body">().template get<"position">();
	pos.at({{0u}}).set(x);
	pos.at({{1u}}).set(y);
}

SAW_TEST("VoxelNarrow / depth matches the analytic oracle"){
	using namespace kel;

	/**
	 * For spheres the answer is known exactly: delta = r_lo + r_hi - |d|.
	 * The voxel result should land within about a voxel pitch of it. This is
	 * the check that the overlap integral is measuring what it claims to.
	 */
	const double radius = 6.0;
	auto g = make_pair(radius);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	for(int step = 1; step <= 4; ++step){
		double sep = 2.0 * radius - step * 1.5;
		place(g, 0u, 32.0 - sep/2.0, 32.0);
		place(g, 1u, 32.0 + sep/2.0, 32.0);

		kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
		bool hit = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m);
		SAW_EXPECT(hit, "expected an overlap");

		double expect = 2.0 * radius - sep;
		double got = m.template get<"depth">().at({}).get();

		SAW_EXPECT(std::abs(got - expect) < 1.6,
			std::string{"sep="} + std::to_string(sep) + " expected depth "
			+ std::to_string(expect) + " got " + std::to_string(got));
	}
}

SAW_TEST("VoxelNarrow / normal points hi to lo"){
	using namespace kel;

	const double radius = 6.0;
	auto g = make_pair(radius);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	place(g, 0u, 28.0, 32.0);
	place(g, 1u, 36.0, 32.0);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
	bool hit = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m);
	SAW_EXPECT(hit, "expected an overlap");

	auto& n = m.template get<"normal">();
	SAW_EXPECT(n.at({{0u}}).get() < -0.9,
		std::string{"normal x should be about -1: "} + std::to_string(n.at({{0u}}).get()));
	SAW_EXPECT(std::abs(n.at({{1u}}).get()) < 0.1,
		std::string{"normal y should be about 0: "} + std::to_string(n.at({{1u}}).get()));

	// Contact point sits between the two centres.
	auto px = m.template get<"point">().at({{0u}}).get();
	SAW_EXPECT(px > 28.0 && px < 36.0,
		std::string{"contact point x out of range: "} + std::to_string(px));
}

SAW_TEST("VoxelNarrow / exactly antisymmetric under pair swap"){
	using namespace kel;

	/**
	 * The reason the lattice is swept rather than either particle's local
	 * grid: both owners must visit identical points in identical order, so
	 * the two halves of the pair cancel bitwise. Sampling on a particle-local
	 * grid would make the sample set depend on which particle you started
	 * from, and every contact would leak a little momentum.
	 */
	const double radius = 5.0;
	auto g = make_pair(radius);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	// Off-axis, so a symmetry accident cannot hide a real asymmetry.
	place(g, 0u, 29.0, 30.0);
	place(g, 1u, 36.0, 33.5);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m01, m10;
	bool a = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m01);
	bool b = (lbm::voxel_contact<sch::T,D>)(g,parts.at({1u}),parts.at({0u}),extent,eps,m10);

	SAW_EXPECT(a && b, "both orderings must find the contact");

	SAW_EXPECT(m01.template get<"depth">().at({}).get() == m10.template get<"depth">().at({}).get(),
		"depth must be identical under swap");

	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(m01.template get<"normal">().at({{i}}).get()
				== -m10.template get<"normal">().at({{i}}).get(),
			std::string{"normal component "} + std::to_string(i) + " not exactly opposite");
		SAW_EXPECT(m01.template get<"point">().at({{i}}).get()
				== m10.template get<"point">().at({{i}}).get(),
			std::string{"contact point component "} + std::to_string(i) + " must match");
	}
}

SAW_TEST("VoxelNarrow / separated bodies report nothing"){
	using namespace kel;

	auto g = make_pair(4.0);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	place(g, 0u, 16.0, 32.0);
	place(g, 1u, 48.0, 32.0);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> m;
	SAW_EXPECT(!(lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,m),
		"well separated bodies must not report a contact");
}

SAW_TEST("VoxelNarrow / agrees with the analytic narrow phase"){
	using namespace kel;

	/**
	 * Same bodies, two independent routes to a manifold. They should agree on
	 * direction closely and on depth to within the voxel pitch; if they do
	 * not, one of the two is wrong about the geometry.
	 */
	const double radius = 6.0;
	auto g = make_pair(radius);
	auto& parts = g.template get<"particles">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,64u}};
	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.0);

	place(g, 0u, 29.0, 31.0);
	place(g, 1u, 36.0, 33.0);

	kel::lbm::data<sch::ContactManifold<sch::T,D>> mv, ma;
	bool hv = (lbm::voxel_contact<sch::T,D>)(g,parts.at({0u}),parts.at({1u}),extent,eps,mv);
	bool ha = (lbm::narrow_phase<lbm::coll::Spheroid<sch::T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), ma);

	SAW_EXPECT(hv && ha, "both narrow phases must find the contact");

	auto dot = mv.template get<"normal">().at({{0u}}).get() * ma.template get<"normal">().at({{0u}}).get()
		+ mv.template get<"normal">().at({{1u}}).get() * ma.template get<"normal">().at({{1u}}).get();
	SAW_EXPECT(dot > 0.97,
		std::string{"normals should nearly agree, dot = "} + std::to_string(dot));

	auto diff = std::abs(mv.template get<"depth">().at({}).get()
		- ma.template get<"depth">().at({}).get());
	SAW_EXPECT(diff < 1.6,
		std::string{"depths disagree by "} + std::to_string(diff));
}
}
