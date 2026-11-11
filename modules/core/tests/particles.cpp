#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>
#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"
#include "../c++/particle/contact.hpp"

namespace {
struct TestDesc2D { static constexpr uint64_t D = 2u; static constexpr uint64_t Q = 9u; };
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}
SAW_TEST("Verlet step 2D - Planar"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> particle;
	auto& body = particle.template get<"rigid_body">();
	auto& pos = body.template get<"position">();
	auto& pos_old = body.template get<"position_old">();

	// auto& rot = body.template get<"rotation">();
	auto& acc = body.template get<"acceleration">();

	acc.at({{0}}).set({1.0});

	// A timestep is a lattice time.
	kel::lbm::data<sch::LbmSecond<sch::Scalar<sch::T>>> dt;
	dt.at({}).set(0.5);
	lbm::verlet_step_lambda<sch::T,2u>(particle,dt);

	SAW_EXPECT(pos.at({{0}}).get() == 0.25, std::string{"Incorrect Pos X: "} + std::to_string(pos.at({{0}}).get()));
	SAW_EXPECT(pos.at({{1}}).get() == 0.0, std::string{"Incorrect Pos Y: "} + std::to_string(pos.at({{1}}).get()));
}
/*
SAW_TEST("No Collision Spheroid 2D"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();

		pos.at({{0u}}) = 0.1;
		pos.at({{1u}}) = 0.2;

	}
	kel::lbm::data<sch::Particle<sch::T,2u>> part_b;
	{
		auto& body = part_b.template get<"rigid_body">();
		auto& pos = body.template get<"position">();

		pos.at({{0u}}) = -2.1;
		pos.at({{1u}}) = 0.0;
	}

	bool have_collided = lbm::broadphase_collision_check<sch::T,2u>(part_a,part_b);
	SAW_EXPECT(not have_collided, "Particles shouldn't collide");
}
*/
/*
SAW_TEST("Collision Spheroid 2D"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& coll = part_a.template get<"collision">();
		auto& radius = coll.template get<"radius">();

		radius.at({}).set(1.0);

		pos.at({{0u}}) = 0.1;
		pos.at({{1u}}) = 0.2;

	}
	kel::lbm::data<sch::Particle<sch::T,2u>> part_b;
	{
		auto& body = part_b.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& coll = part_b.template get<"collision">();
		auto& radius = coll.template get<"radius">();

		radius.at({}).set(1.0);
		
		pos.at({{0u}}) = -1.5;
		pos.at({{1u}}) = 0.0;
	}

	bool have_collided = lbm::broadphase_collision_check<sch::T,2u>(part_a,part_b);
	SAW_EXPECT(have_collided, "Particles should collide");
}
*/
/*
SAW_TEST("Moving particles 2D"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();
		auto& coll = part_a.template get<"collision">();
		auto& radius = coll.template get<"radius">();
		auto& acc = body.template get<"acceleration">();

		radius.at({}).set(1.0);

		pos.at({{0u}}) = -5.0;
		pos.at({{1u}}) = 0.2;
		pos_old = pos;
		acc.at({{0u}}) = 0.1;

	}
	kel::lbm::data<sch::Particle<sch::T,2u>> part_b;
	{
		auto& body = part_b.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();
		auto& coll = part_b.template get<"collision">();
		auto& radius = coll.template get<"radius">();
		auto& acc = body.template get<"acceleration">();

		radius.at({}).set(1.0);
		
		pos.at({{0u}}) = 5.0;
		pos.at({{1u}}) = 0.0;
		pos_old = pos;
		acc.at({{0u}}) = -0.1;
	}

	// A timestep is a lattice time.
	kel::lbm::data<sch::LbmSecond<sch::Scalar<sch::T>>> dt;
	dt.at({}).set(0.5);
	bool has_collided = false;
	for(uint64_t i = 0u; i < 32u; ++i){
		lbm::verlet_step_lambda<sch::T,2u>(part_a,dt);
		lbm::verlet_step_lambda<sch::T,2u>(part_b,dt);

		has_collided = lbm::broadphase_collision_check<sch::T,2u>(part_a,part_b);
		if(has_collided){
			std::cout<<"Collided at: "<<i<<std::endl;
			break;
		}
	}

	SAW_EXPECT(has_collided, "Expecting collision within those steps");
}
*/
SAW_TEST("Particle Matrix Rotation"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();
		auto& acc = body.template get<"acceleration">();

		pos.at({{0u}}) = -5.0;
		pos.at({{1u}}) = 0.2;
		pos_old = pos;
	}
}
/*

SAW_TEST("Particle Collision Impulse"){
	using namespace kel;

	kel::lbm::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();
		auto& coll = part_a.template get<"collision">();
		auto& radius = coll.template get<"radius">();
		auto& acc = body.template get<"acceleration">();

		radius.at({}).set(1.0);

		pos.at({{0u}}) = -5.0;
		pos.at({{1u}}) = 0.2;
		pos_old = pos;
		acc.at({{0u}}) = 0.1;

	}
	kel::lbm::data<sch::Particle<sch::T,2u>> part_b;
	{
		auto& body = part_b.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();
		auto& coll = part_b.template get<"collision">();
		auto& radius = coll.template get<"radius">();
		auto& acc = body.template get<"acceleration">();

		radius.at({}).set(1.5);
		
		pos.at({{0u}}) = 5.0;
		pos.at({{1u}}) = 0.0;
		pos_old = pos;
		acc.at({{0u}}) = -0.1;
	}
}
*/
/*

SAW_TEST("Minor Test for mask"){
	using namespace kel;

	lbm::particle_circle_geometry<sch::T> geo;

	auto mask = geo.generate_mask<sch::T>(9u,1u);

	auto& grid = mask.template get<"grid">();

	for(kel::lbm::data<sch::UInt64> i{0u}; i < grid.template get_dim_size<0>(); ++i){
		for(kel::lbm::data<sch::UInt64> j{0u}; j < grid.template get_dim_size<1>(); ++j){
			std::cout<<grid.at({{i,j}}).get()<<" ";
		}
		std::cout<<"\n";
	}
	std::cout<<std::endl;

	//kel::lbm::data<sch::Array<sch::T,2>> reference_mask{{{4+2,4+2}}};
	//reference_mask.at({{0,0}});
}
*/
/*
SAW_TEST("Verlet integration test 2D"){
	using namespace kel;
	lbm::particle_system<sch::T,2,sch::Particle<sch::T,2>> system;

	{
		kel::lbm::data<sch::Particle<sch::T,2>> particle;
		auto& rb = particle.template get<"rigid_body">();
		auto& acc = rb.template get<"acceleration">();
		auto& pos = rb.template get<"position">();
		auto& pos_old = rb.template get<"position_old">();
		pos = {{1e-1,1e-1}};
		pos_old = {{0.0, 0.0}};
		acc = {{0.0,-1e1}};

		auto eov = system.add_particle(std::move(particle));
		SAW_EXPECT(eov.is_value(), "Expected no error :)");
	}
	{
		auto& p = system.at({0u});
		auto& rb = p.template get<"rigid_body">();
		auto& pos = rb.template get<"position">();

		for(kel::lbm::data<sch::UInt64> i{0u}; i < kel::lbm::data<sch::UInt64>{2u}; ++i){
			std::cout<<pos.at(i).get()<<" ";
		}
		std::cout<<std::endl;
	}

	for(uint64_t i = 0u; i < 36u; ++i){
		system.step(kel::lbm::data<sch::T>{1e-1});

		{
			auto& p = system.at({0u});
			auto& rb = p.template get<"rigid_body">();
			auto& pos = rb.template get<"position">();

			for(kel::lbm::data<sch::UInt64> i{0u}; i < kel::lbm::data<sch::UInt64>{2u}; ++i){
				std::cout<<pos.at(i).get()<<" ";
			}
			std::cout<<"\n";

			if(pos.at({1u}).get() < 0.0){
				break;
			}
		}

	}
}
*/

SAW_TEST("Spheroid Particle / AABB"){
	using namespace kel;

	using T = sch::Float64;
	constexpr uint64_t D = 2u;
	constexpr uint64_t PC = 1u;

	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> radi;
	kel::lbm::data<sch::Scalar<T>> dense;
	radi.at({}) = 2.2f;
	dense.at({}) = 1.0f;
	auto spheroid_pgrp = lbm::create_spheroid_particle_group<T,D,PC>(radi,dense,{8u});

	auto& rb_array = spheroid_pgrp.template get<"particles">();
	auto& rb = rb_array.at({0u}).template get<"rigid_body">();

	auto& rb_pos = rb.template get<"position">();
	{
		rb_pos.at({{0u}}).set(4.1f);
		rb_pos.at({{1u}}).set(2.9f);
	}

	auto eo_res = lbm::particle_aabb<lbm::sch::ParticleGroup<T,D,PC,lbm::coll::Spheroid<T>>>::calculate(spheroid_pgrp,{{0u}},{{64u,32u}});
	SAW_EXPECT(eo_res.is_value(), "particle_aabb has error value");
	auto& res = eo_res.get_value();
	{
		auto res_check = [&](const auto& tbp){
			lbm::iterator<1u>::apply([&](const auto& index){
				std::cout<<tbp.at(index).get()<<" ";
			},{{0u}},{{D}},{{0u}});
			std::cout<<std::endl;
		};

		res_check(res.template get<"a">());
		res_check(res.template get<"b">());
	}
	auto& rva = res.template get<"a">();
	auto& rvb = res.template get<"b">();

	SAW_EXPECT(rva.at({{0u}}).get() == 1, std::string{"Expected another value for rva at 0: "} + std::to_string(rva.at({{0u}}).get()));
	SAW_EXPECT(rva.at({{1u}}).get() == 0, std::string{"Expected another value for rva at 1: "} + std::to_string(rva.at({{1u}}).get()));
	
	SAW_EXPECT(rvb.at({{0u}}).get() == 7, std::string{"Expected another value for rvb at 0: "} + std::to_string(rvb.at({{0u}}).get()));
	SAW_EXPECT(rvb.at({{1u}}).get() == 6, std::string{"Expected another value for rvb at 1: "} + std::to_string(rvb.at({{1u}}).get()));

	// std::cout<<res<<std::endl;
}

SAW_TEST("Spheroid Particle / AABB per particle"){
	using namespace kel;

	using T = sch::Float64;
	constexpr uint64_t D = 2u;
	constexpr uint64_t PC = 3u;

	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> radi;
	kel::lbm::data<sch::Scalar<T>> dense;
	radi.at({}) = 2.2f;
	dense.at({}) = 1.0f;
	auto pgrp = lbm::create_spheroid_particle_group<T,D,PC>(radi,dense,{8u});

	auto& parts = pgrp.template get<"particles">();

	auto place = [&](uint64_t i, double x, double y){
		auto& pos = parts.at({i}).template get<"rigid_body">().template get<"position">();
		pos.at({{0u}}).set(x);
		pos.at({{1u}}).set(y);
	};

	place(0u,  4.1,  2.9);
	place(1u, 40.0, 20.0);
	place(2u, 63.0, 31.0);   // hard against the far wall

	using PG = lbm::sch::ParticleGroup<T,D,PC,lbm::coll::Spheroid<T>>;
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> meta{{64u,32u}};

	// Each particle must get its OWN box. Before the fix every call site
	// handed particle 0's index in, so this returned particle 0's box.
	auto eo_1 = lbm::particle_aabb<PG>::calculate(pgrp,{{1u}},meta);
	SAW_EXPECT(eo_1.is_value(), "particle_aabb errored for particle 1");
	auto& a1 = eo_1.get_value().template get<"a">();
	auto& b1 = eo_1.get_value().template get<"b">();

	SAW_EXPECT(a1.at({{0u}}).get() == 37u,
		std::string{"particle 1 a.x: "} + std::to_string(a1.at({{0u}}).get()));
	SAW_EXPECT(a1.at({{1u}}).get() == 17u,
		std::string{"particle 1 a.y: "} + std::to_string(a1.at({{1u}}).get()));
	SAW_EXPECT(b1.at({{0u}}).get() == 43u,
		std::string{"particle 1 b.x: "} + std::to_string(b1.at({{0u}}).get()));
	SAW_EXPECT(b1.at({{1u}}).get() == 23u,
		std::string{"particle 1 b.y: "} + std::to_string(b1.at({{1u}}).get()));

	// The far-wall particle must be clamped to the domain. calculate() takes
	// meta and, before the fix, never looked at it.
	auto eo_2 = lbm::particle_aabb<PG>::calculate(pgrp,{{2u}},meta);
	SAW_EXPECT(eo_2.is_value(), "particle_aabb errored for particle 2");
	auto& b2 = eo_2.get_value().template get<"b">();

	SAW_EXPECT(b2.at({{0u}}).get() == 64u,
		std::string{"particle 2 b.x should clamp to 64: "} + std::to_string(b2.at({{0u}}).get()));
	SAW_EXPECT(b2.at({{1u}}).get() == 32u,
		std::string{"particle 2 b.y should clamp to 32: "} + std::to_string(b2.at({{1u}}).get()));

	// Out of range index is still an error, not a silent clamp.
	auto eo_bad = lbm::particle_aabb<PG>::calculate(pgrp,{{PC}},meta);
	SAW_EXPECT(eo_bad.is_error(), "out of range particle index must error");
}

SAW_TEST("Spheroid Particle / per-particle inertial fan-out"){
	using namespace kel;

	using T = sch::Float64;
	constexpr uint64_t D = 2u;
	constexpr uint64_t PC = 4u;

	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> radi;
	kel::lbm::data<sch::Scalar<T>> dense;
	radi.at({}) = 2.0;
	dense.at({}) = 3.0;
	auto pgrp = lbm::create_spheroid_particle_group<T,D,PC>(radi,dense,{8u});

	auto& parts = pgrp.template get<"particles">();

	constexpr double pi = 3.14159265358979323846;
	const double expect_m = pi * 2.0 * 2.0 * 3.0;      // pi r^2 rho
	const double expect_i = 0.5 * expect_m * 2.0 * 2.0; // m r^2 / 2

	// Every particle must carry its own copy, not just particle 0.
	for(uint64_t i = 0u; i < PC; ++i){
		auto& pi_p = parts.at({i});
		auto& rb = pi_p.template get<"rigid_body">();

		SAW_EXPECT(pi_p.template get<"collision">().template get<"radius">().at({}).get() == 2.0,
			std::string{"radius not fanned out at "} + std::to_string(i));
		SAW_EXPECT(rb.template get<"density">().at({}).get() == 3.0,
			std::string{"density not fanned out at "} + std::to_string(i));

		auto m = rb.template get<"mass">().at({}).get();
		SAW_EXPECT(std::abs(m - expect_m) < 1e-9,
			std::string{"mass at "} + std::to_string(i) + ": " + std::to_string(m));

		auto in = rb.template get<"inertia">().at({}).get();
		SAW_EXPECT(std::abs(in - expect_i) < 1e-9,
			std::string{"inertia at "} + std::to_string(i) + ": " + std::to_string(in));
	}

	// Accumulators start clean.
	auto& rb0 = parts.at({0u}).template get<"rigid_body">();
	SAW_EXPECT(rb0.template get<"force">().at({{0u}}).get() == 0.0, "force must start zeroed");
	SAW_EXPECT(rb0.template get<"torque">().at({}).get() == 0.0, "torque must start zeroed");

	// Sizes are now independent per particle.
	parts.at({1u}).template get<"collision">().template get<"radius">().at({}).set(5.0);
	SAW_EXPECT(parts.at({0u}).template get<"collision">().template get<"radius">().at({}).get() == 2.0,
		"changing one particle's radius must not touch another's");
}

SAW_TEST("Spheroid Particle / broadphase uses per-particle radii"){
	using namespace kel;

	using T = sch::Float64;
	constexpr uint64_t D = 2u;

	kel::lbm::data<sch::Particle<T,D>> a, b;

	auto set = [](auto& p, double x, double r){
		p.template get<"rigid_body">().template get<"position">().at({{0u}}).set(x);
		p.template get<"collision">().template get<"radius">().at({}).set(r);
	};

	// Centres 10 apart. Radii 2+2 do not reach; 2+9 do.
	set(a, 0.0, 2.0);
	set(b, 10.0, 2.0);
	SAW_EXPECT((!lbm::broadphase_collision_check<T,D>(a,b)), "r=2,2 at distance 10 must not overlap");

	set(b, 10.0, 9.0);
	SAW_EXPECT((lbm::broadphase_collision_check<T,D>(a,b)), "r=2,9 at distance 10 must overlap");

	// Squared centre distance comes back for the narrow phase to reuse.
	auto res = (lbm::broadphase_collision_distance_squared<T,D>)(a,b);
	SAW_EXPECT(std::abs(res.second.at({}).get() - 100.0) < 1e-9,
		std::string{"expected squared distance 100: "} + std::to_string(res.second.at({}).get()));
}

SAW_TEST("Particle / force accumulates then integrates"){
	using namespace kel;
	using T = sch::Float64;
	constexpr uint64_t D = 2u;
	constexpr uint64_t PC = 2u;



	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> radi;
	kel::lbm::data<sch::Scalar<T>> dense;
	radi.at({}) = 1.0;
	dense.at({}) = 1.0 / 3.14159265358979323846;   // gives mass == 1
	auto pgrp = lbm::create_spheroid_particle_group<T,D,PC>(radi,dense,{4u});

	auto& rb = pgrp.template get<"particles">().at({0u}).template get<"rigid_body">();

	
	

	lbm::reset_particle_accumulators<T,D>(pgrp.template get<"particles">().at({0u}));
	SAW_EXPECT(rb.template get<"force">().at({{0u}}).get() == 0.0, "reset must zero force");

	// Two contributors must sum, not overwrite each other.
	auto& f = rb.template get<"force">();
	f.at({{0u}}).set(f.at({{0u}}).get() + 3.0);
	f.at({{0u}}).set(f.at({{0u}}).get() + 1.0);
	SAW_EXPECT(f.at({{0u}}).get() == 4.0, "contributors must accumulate");

	// mass == 1, so acceleration == force. One step of x = 2x - x_old + a dt^2.
	(lbm::integrate_particle<T,D>)(pgrp.template get<"particles">().at({0u}), {}, {1u});
	SAW_EXPECT(std::abs(rb.template get<"acceleration">().at({{0u}}).get() - 4.0) < 1e-9,
		"acceleration must come from accumulated force");
	SAW_EXPECT(std::abs(rb.template get<"position">().at({{0u}}).get() - 4.0) < 1e-9,
		std::string{"expected x=4 after one unit step: "}
		+ std::to_string(rb.template get<"position">().at({{0u}}).get()));
}

SAW_TEST("Particle / zero sub steps still advances"){
	using namespace kel;
	using T = sch::Float64;
	constexpr uint64_t D = 2u;
	constexpr uint64_t PC = 1u;



	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> radi;
	kel::lbm::data<sch::Scalar<T>> dense;
	radi.at({}) = 1.0;
	dense.at({}) = 1.0 / 3.14159265358979323846;
	auto pgrp = lbm::create_spheroid_particle_group<T,D,PC>(radi,dense,{4u});

	auto& rb = pgrp.template get<"particles">().at({0u}).template get<"rigid_body">();
	rb.template get<"force">().at({{0u}}).set(2.0);

	

	/**
	 * sub_steps == 0 used to give ts = 1/0 = inf and a loop that never ran,
	 * so the particle silently froze. Several examples pass {0u}.
	 */
	(lbm::integrate_particle<T,D>)(pgrp.template get<"particles">().at({0u}), {}, {0u});

	SAW_EXPECT(rb.template get<"position">().at({{0u}}).get() != 0.0,
		"zero sub steps must still advance the particle");
	SAW_EXPECT(std::isfinite(rb.template get<"position">().at({{0u}}).get()),
		"position must stay finite");
}

namespace contact_fx {
using T = sch::Float64;
constexpr uint64_t D = 2u;

template<uint64_t PC>
auto make_group(double radius, double density){
	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> r;
	kel::lbm::data<sch::Scalar<T>> d;
	r.at({}).set(radius);
	d.at({}).set(density);
	return kel::lbm::create_spheroid_particle_group<T,D,PC>(r,d,{4u});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y, double ox = 0.0, double oy = 0.0){
	auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(x);
	rb.template get<"position">().at({{1u}}).set(y);
	rb.template get<"position_old">().at({{0u}}).set(x - ox);
	rb.template get<"position_old">().at({{1u}}).set(y - oy);
}
}

SAW_TEST("Contact / manifold geometry"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<2u>(2.0, 1.0);
	place(g, 0u, 0.0, 0.0);
	place(g, 1u, 3.0, 0.0);

	auto& parts = g.template get<"particles">();
	kel::lbm::data<sch::ContactManifold<T,D>> m;

	bool hit = (lbm::narrow_phase<lbm::coll::Spheroid<T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);

	SAW_EXPECT(hit, "r=2,2 at distance 3 must be in contact");
	SAW_EXPECT(std::abs(m.template get<"depth">().at({}).get() - 1.0) < 1e-12,
		std::string{"depth: "} + std::to_string(m.template get<"depth">().at({}).get()));
	// normal points from hi (particle 1, at +x) back toward lo (particle 0)
	SAW_EXPECT(std::abs(m.template get<"normal">().at({{0u}}).get() + 1.0) < 1e-12,
		"normal must point from hi toward lo");
	SAW_EXPECT(std::abs(m.template get<"point">().at({{0u}}).get() - 1.5) < 1e-12,
		std::string{"contact point x: "} + std::to_string(m.template get<"point">().at({{0u}}).get()));
}

SAW_TEST("Contact / exact antisymmetry"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<2u>(2.0, 1.0);
	// Approaching each other, off-axis so the tangential path is exercised.
	place(g, 0u, 0.0, 0.3,  0.05, 0.0);
	place(g, 1u, 3.0, 0.0, -0.05, 0.0);

	lbm::dem_params<T> dem{10.0, 0.4, 2.0, 0.5};

	auto& parts = g.template get<"particles">();
	kel::lbm::data<sch::ContactManifold<T,D>> m;
	bool hit = (lbm::narrow_phase<lbm::coll::Spheroid<T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);
	SAW_EXPECT(hit, "expected contact");

	auto r0 = (lbm::particle_contact_response<T,D>)(m, parts.at({0u}), parts.at({1u}), true,  dem, {1u});
	auto r1 = (lbm::particle_contact_response<T,D>)(m, parts.at({1u}), parts.at({0u}), false, dem, {1u});

	/**
	 * Bitwise, not approximate. An epsilon here would hide exactly the
	 * ordering mistake this test exists to catch: if the two owners evaluate
	 * the pair in different orders the halves differ by a rounding step and
	 * every contact leaks momentum.
	 */
	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(r0.force.at({{i}}).get() == -r1.force.at({{i}}).get(),
			std::string{"force component "} + std::to_string(i) + " not exactly opposite: "
			+ std::to_string(r0.force.at({{i}}).get()) + " vs "
			+ std::to_string(r1.force.at({{i}}).get()));
	}
}

SAW_TEST("Contact / never attracts"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<2u>(2.0, 1.0);
	// Overlapping but separating fast: the dashpot alone would pull them back.
	place(g, 0u, 0.0, 0.0, -1.0, 0.0);
	place(g, 1u, 3.0, 0.0,  1.0, 0.0);

	lbm::dem_params<T> dem{1.0, 0.9, 0.0, 0.0};

	auto& parts = g.template get<"particles">();
	kel::lbm::data<sch::ContactManifold<T,D>> m;
	(void)(lbm::narrow_phase<lbm::coll::Spheroid<T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);

	auto r0 = (lbm::particle_contact_response<T,D>)(m, parts.at({0u}), parts.at({1u}), true, dem, {1u});

	// Particle 0 sits at -x, so any push on it must be toward -x, never +x.
	SAW_EXPECT(r0.force.at({{0u}}).get() <= 0.0,
		std::string{"separating pair must not be pulled together: "}
		+ std::to_string(r0.force.at({{0u}}).get()));
}

SAW_TEST("Contact / degenerate pair stays finite"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<2u>(2.0, 1.0);
	// Exactly coincident centres: no geometric normal exists.
	place(g, 0u, 5.0, 5.0);
	place(g, 1u, 5.0, 5.0);

	lbm::dem_params<T> dem{1.0, 0.3, 0.0, 0.0};

	auto& parts = g.template get<"particles">();
	kel::lbm::data<sch::ContactManifold<T,D>> m;
	bool hit = (lbm::narrow_phase<lbm::coll::Spheroid<T>>::template contact<D>)(
		parts.at({0u}), parts.at({1u}), m);
	SAW_EXPECT(hit, "coincident centres are in contact");

	auto r0 = (lbm::particle_contact_response<T,D>)(m, parts.at({0u}), parts.at({1u}), true, dem, {1u});

	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(std::isfinite(r0.force.at({{i}}).get()),
			"degenerate contact must not produce NaN");
	}
	// Penetration is capped at the smaller radius, not 2r.
	SAW_EXPECT(std::abs(m.template get<"depth">().at({}).get() - 2.0) < 1e-12,
		std::string{"depth should cap at the radius: "}
		+ std::to_string(m.template get<"depth">().at({}).get()));
}

SAW_TEST("Contact / pair loop conserves momentum"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<2u>(2.0, 1.0);
	place(g, 0u, 0.0, 0.4,  0.1, 0.0);
	place(g, 1u, 3.0, 0.0, -0.1, 0.0);

	lbm::dem_params<T> dem{10.0, 0.4, 1.0, 0.3};

	auto& parts = g.template get<"particles">();
	for(uint64_t i = 0u; i < 2u; ++i){
		lbm::reset_particle_accumulators<T,D>(parts.at({i}));
	}
	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::accumulate_particle_contacts<T,D>)(g.template get<"particles">(), i, dem, {1u});
	}

	// Each owner computed only its own half; the halves must cancel exactly.
	for(uint64_t k = 0u; k < D; ++k){
		auto f0 = parts.at({0u}).template get<"rigid_body">().template get<"force">().at({{k}}).get();
		auto f1 = parts.at({1u}).template get<"rigid_body">().template get<"force">().at({{k}}).get();
		SAW_EXPECT(f0 == -f1,
			std::string{"net force must be zero in component "} + std::to_string(k)
			+ ": " + std::to_string(f0) + " vs " + std::to_string(f1));
	}
}

SAW_TEST("Contact / wall soft shell"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<1u>(2.0, 1.0);
	// Overlapping the low x wall by 0.5, drifting further in.
	place(g, 0u, 1.5, 10.0, -0.1, 0.0);

	lbm::dem_params<T> dem{4.0, 0.0, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	auto& parts = g.template get<"particles">();
	lbm::reset_particle_accumulators<T,D>(parts.at({0u}));
	(lbm::accumulate_box_wall_contacts<T,D>)(parts.at({0u}), extent, dem, {1u});

	auto fx = parts.at({0u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get();
	// Undamped: f = k * depth = 4 * 0.5 = 2, pushing inward (+x).
	SAW_EXPECT(std::abs(fx - 2.0) < 1e-12,
		std::string{"wall push: "} + std::to_string(fx));

	// Well clear of every wall means no contact at all.
	auto g2 = make_group<1u>(2.0, 1.0);
	place(g2, 0u, 32.0, 16.0);
	auto& p2 = g2.template get<"particles">();
	lbm::reset_particle_accumulators<T,D>(p2.at({0u}));
	(lbm::accumulate_box_wall_contacts<T,D>)(p2.at({0u}), extent, dem, {1u});
	SAW_EXPECT(p2.at({0u}).template get<"rigid_body">().template get<"force">().at({{0u}}).get() == 0.0,
		"a particle away from the walls must feel nothing");
}

SAW_TEST("Contact / corner touches both axes"){
	using namespace kel;
	using namespace contact_fx;

	auto g = make_group<1u>(2.0, 1.0);
	place(g, 0u, 1.0, 1.0);   // inside both the low x and low y walls

	lbm::dem_params<T> dem{4.0, 0.0, 0.0, 0.0};
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{64u,32u}};

	auto& parts = g.template get<"particles">();
	lbm::reset_particle_accumulators<T,D>(parts.at({0u}));
	(lbm::accumulate_box_wall_contacts<T,D>)(parts.at({0u}), extent, dem, {1u});

	auto& f = parts.at({0u}).template get<"rigid_body">().template get<"force">();
	SAW_EXPECT(f.at({{0u}}).get() > 0.0, "corner must push in +x");
	SAW_EXPECT(f.at({{1u}}).get() > 0.0, "corner must push in +y");
}
}
