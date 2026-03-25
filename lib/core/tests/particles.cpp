#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/particle/particle.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}
SAW_TEST("Verlet step 2D - Planar"){
	using namespace kel;

	saw::data<sch::Particle<sch::T,2u>> particle;
	auto& body = particle.template get<"rigid_body">();
	auto& pos = body.template get<"position">();
	auto& pos_old = body.template get<"position_old">();

	// auto& rot = body.template get<"rotation">();
	auto& acc = body.template get<"acceleration">();

	acc.at({{0}}).set({1.0});

	saw::data<sch::Scalar<sch::T>> dt;
	dt.at({}).set(0.5);
	lbm::verlet_step_lambda<sch::T,2u>(particle,dt);

	SAW_EXPECT(pos.at({{0}}).get() == 0.25, std::string{"Incorrect Pos X: "} + std::to_string(pos.at({{0}}).get()));
	SAW_EXPECT(pos.at({{1}}).get() == 0.0, std::string{"Incorrect Pos Y: "} + std::to_string(pos.at({{1}}).get()));
}
/*
SAW_TEST("No Collision Spheroid 2D"){
	using namespace kel;

	saw::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();

		pos.at({{0u}}) = 0.1;
		pos.at({{1u}}) = 0.2;

	}
	saw::data<sch::Particle<sch::T,2u>> part_b;
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

	saw::data<sch::Particle<sch::T,2u>> part_a;
	{
		auto& body = part_a.template get<"rigid_body">();
		auto& pos = body.template get<"position">();
		auto& coll = part_a.template get<"collision">();
		auto& radius = coll.template get<"radius">();

		radius.at({}).set(1.0);

		pos.at({{0u}}) = 0.1;
		pos.at({{1u}}) = 0.2;

	}
	saw::data<sch::Particle<sch::T,2u>> part_b;
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

	saw::data<sch::Particle<sch::T,2u>> part_a;
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
	saw::data<sch::Particle<sch::T,2u>> part_b;
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

	saw::data<sch::Scalar<sch::T>> dt;
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

	saw::data<sch::Particle<sch::T,2u>> part_a;
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

	saw::data<sch::Particle<sch::T,2u>> part_a;
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
	saw::data<sch::Particle<sch::T,2u>> part_b;
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

	for(saw::data<sch::UInt64> i{0u}; i < grid.template get_dim_size<0>(); ++i){
		for(saw::data<sch::UInt64> j{0u}; j < grid.template get_dim_size<1>(); ++j){
			std::cout<<grid.at({{i,j}}).get()<<" ";
		}
		std::cout<<"\n";
	}
	std::cout<<std::endl;

	//saw::data<sch::Array<sch::T,2>> reference_mask{{{4+2,4+2}}};
	//reference_mask.at({{0,0}});
}
*/
/*
SAW_TEST("Verlet integration test 2D"){
	using namespace kel;
	lbm::particle_system<sch::T,2,sch::Particle<sch::T,2>> system;

	{
		saw::data<sch::Particle<sch::T,2>> particle;
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

		for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{2u}; ++i){
			std::cout<<pos.at(i).get()<<" ";
		}
		std::cout<<std::endl;
	}

	for(uint64_t i = 0u; i < 36u; ++i){
		system.step(saw::data<sch::T>{1e-1});

		{
			auto& p = system.at({0u});
			auto& rb = p.template get<"rigid_body">();
			auto& pos = rb.template get<"position">();

			for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{2u}; ++i){
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
}
