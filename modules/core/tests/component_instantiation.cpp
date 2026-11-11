#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/abstract/data_ptr.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

struct TestDesc { static constexpr uint64_t D = 2u; static constexpr uint64_t Q = 9u; };
constexpr uint64_t D = 2u;

/**
 * The components are what a step function actually launches, but they take
 * their group through a borrowed view -- const handle, mutable target -- so
 * they could not be instantiated against the honestly const-correct owning
 * types. Templates are only checked on instantiation, so a component naming a
 * member that does not exist stayed quiet until a real kernel compiled it.
 * Twice, here: ParticleSchema::PartAmount and CellListSchema::CellCapacity.
 *
 * With a host view carrying the same const semantics, the wrappers can be
 * exercised directly.
 */

using GroupSch = kel::lbm::sch::ParticleGroup<sch::T,D,2u,kel::lbm::coll::Spheroid<sch::T>>;
using GroupView = kel::lbm::data<sch::Ptr<GroupSch>, kel::lbm::enc::Native>;

auto make_group(double radius){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>> r;
	kel::lbm::data<sch::Scalar<sch::T>> d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,2u>(r,d,{8u});
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

SAW_TEST("Component / host view keeps device const semantics"){
	using namespace kel;

	/**
	 * The whole point: a const view must still allow writing through it, the
	 * way a device handle does.
	 */
	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();

	const kel::lbm::data<sch::Ptr<sch::FixedArray<
		sch::Particle<sch::T,D,lbm::coll::Spheroid<sch::T>>, 2u>>, lbm::enc::Native> view{parts};

	view.at({0u}).template get<"rigid_body">()
		.template get<"position">().at({{0u}}).set(7.0);

	SAW_EXPECT(parts.at({0u}).template get<"rigid_body">()
		.template get<"position">().at({{0u}}).get() == 7.0,
		"a write through a const view must reach the owning data");

	SAW_EXPECT(view.get_dims().at({0u}).get() == 2u,
		"the view must report the backing extent");
}

SAW_TEST("Component / ParticleForceReset instantiates and clears"){
	using namespace kel;

	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();

	auto& rb = parts.at({0u}).template get<"rigid_body">();
	rb.template get<"force">().at({{0u}}).set(5.0);
	rb.template get<"torque">().at({}).set(3.0);

	lbm::component<sch::T,TestDesc,lbm::cmpt::ParticleForceReset,lbm::enc::Native> reset;
	GroupView view{g};
	reset.apply(view,{{0u}},{0u});

	SAW_EXPECT(rb.template get<"force">().at({{0u}}).get() == 0.0, "force must clear");
	SAW_EXPECT(rb.template get<"torque">().at({}).get() == 0.0, "torque must clear");
}

SAW_TEST("Component / ParticleIntegrate instantiates and moves the body"){
	using namespace kel;

	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();
	rest_at(g, 0u, 0.0, 0.0);

	auto& rb = parts.at({0u}).template get<"rigid_body">();
	auto mass = rb.template get<"mass">().at({}).get();
	rb.template get<"force">().at({{0u}}).set(mass);   // unit acceleration

	// Gravity is an acceleration, and now has to say so.
	kel::lbm::data<sch::LbmAcceleration<sch::Vector<sch::T,D>>> gravity;
	gravity.at({{1u}}).set(-0.5);

	lbm::component<sch::T,TestDesc,lbm::cmpt::ParticleIntegrate,lbm::enc::Native> integrate{gravity};
	GroupView view{g};
	integrate.apply(view,{{0u}},{0u},{1u});

	SAW_EXPECT(std::abs(rb.template get<"acceleration">().at({{0u}}).get() - 1.0) < 1e-12,
		"force/mass must reach acceleration");
	SAW_EXPECT(std::abs(rb.template get<"acceleration">().at({{1u}}).get() + 0.5) < 1e-12,
		"the body acceleration must be added in");
	SAW_EXPECT(rb.template get<"position">().at({{0u}}).get() > 0.0, "and the body must move");
}

SAW_TEST("Component / ParticleContact instantiates and accumulates"){
	using namespace kel;

	/**
	 * This is the component that previously only compiled once a GPU kernel
	 * referenced it -- the PartAmount slip lived exactly here.
	 */
	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();

	rest_at(g, 0u, 20.0, 16.0);
	rest_at(g, 1u, 23.0, 16.0);   // overlapping by 1

	lbm::dem_params<sch::T> dem{4.0, 0.0, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	lbm::component<sch::T,TestDesc,lbm::cmpt::ParticleContact,lbm::enc::Native> contact{dem,extent};

	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
	}
	GroupView view{g};
	contact.apply(view,{{0u}},{0u},{1u});
	contact.apply(view,{{1u}},{0u},{1u});

	auto f0 = parts.at({0u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get();
	auto f1 = parts.at({1u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get();

	// Undamped, overlap 1, k = 4, so the pair pushes apart with magnitude 4.
	SAW_EXPECT(std::abs(f0 + 4.0) < 1e-12,
		std::string{"particle 0 should be pushed -x by k*delta: "} + std::to_string(f0));
	SAW_EXPECT(f0 == -f1, "and the halves must cancel exactly");
}

SAW_TEST("Component / wall contact runs through the component too"){
	using namespace kel;

	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();

	// Against the low x wall, far from the other particle.
	rest_at(g, 0u, 1.5, 16.0);
	rest_at(g, 1u, 50.0, 16.0);

	const double k_n = 6.0;
	lbm::dem_params<sch::T> dem{k_n, 0.0, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	lbm::component<sch::T,TestDesc,lbm::cmpt::ParticleContact,lbm::enc::Native> contact{dem,extent};

	(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
	GroupView view{g};
	contact.apply(view,{{0u}},{0u},{1u});

	auto fx = parts.at({0u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get();
	SAW_EXPECT(std::abs(fx - k_n * 0.5) < 1e-12,
		std::string{"wall should push +x by k*depth: "} + std::to_string(fx));
}

SAW_TEST("Component / contact without walls leaves the wall term out"){
	using namespace kel;

	// The two-argument constructor opts out of wall contact; a particle
	// overlapping a wall must then feel nothing from it.
	auto g = make_group(2.0);
	auto& parts = g.template get<"particles">();
	rest_at(g, 0u, 1.5, 16.0);
	rest_at(g, 1u, 50.0, 16.0);

	lbm::dem_params<sch::T> dem{6.0, 0.0, 0.0, 0.0};
	lbm::component<sch::T,TestDesc,lbm::cmpt::ParticleContact,lbm::enc::Native> contact{dem};

	(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({0u}));
	GroupView view{g};
	contact.apply(view,{{0u}},{0u},{1u});

	SAW_EXPECT(parts.at({0u}).template get<"rigid_body">()
		.template get<"force">().at({{0u}}).get() == 0.0,
		"no walls configured means no wall force");
}
}
