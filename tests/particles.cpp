#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/particle/geometry/circle.hpp"


namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}

SAW_TEST("Minor Test for mask"){
	using namespace kel;

	lbm::particle_circle_geometry<sch::T> geo;

	auto mask = geo.generate_mask<sch::T>(4,2);

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

SAW_TEST("Verlet integration test"){
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

	for(uint64_t i = 0u; i < 360u; ++i){
		system.step(saw::data<sch::T>{1e-2});

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
}
