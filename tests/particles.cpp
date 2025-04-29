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
}
