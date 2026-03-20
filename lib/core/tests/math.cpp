#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/math/n_linear.hpp"

namespace {
namespace sch {
using namespace saw::schema;
}

SAW_TEST("Math 1-Linear"){
	using namespace kel;
	
	saw::data<sch::FixedArray<sch::Vector<sch::Float64,1u>,2u>> field;
	{
		field.at({{0u}}).at({{0u}}).set(-1.0);
		field.at({{1u}}).at({{0u}}).set(2.0);
	}
	saw::data<sch::Vector<sch::Float64,1u>> pos;
	pos.at({{0u}}).set(0.3);
}

SAW_TEST("Math/Floor Index and Fraction from Position"){
	using namespace kel;

	saw::data<sch::Vector<sch::Float64,2u>> pos;

	{
		pos.at({{0u}}) = 43.999;
		pos.at({{1u}}) = -50.0;
	}

	auto ind_frac = lbm::position_to_index_and_fraction(pos);
	auto& ind = ind_frac.template get<0u>();
	for(uint64_t i = 0u; i < 2u; ++i){
		std::cout<<ind.at({{i}}).get()<<" ";
	}
	std::cout<<std::endl;
	auto& frac = ind_frac.template get<1u>();
	for(uint64_t i = 0u; i < 2u; ++i){
		std::cout<<frac.at({{i}}).get()<<" ";
	}
	std::cout<<std::endl;

	SAW_EXPECT(true, "Default true check");
}

SAW_TEST("Math/Floor Index and Fraction from Position and Bounded"){
	using namespace kel;

	saw::data<sch::Vector<sch::Float64,2u>> pos;

	{
		pos.at({{0u}}) = 43.999;
		pos.at({{1u}}) = -50.0;
	}

	saw::data<sch::Vector<sch::UInt64,2U>> bound;
	{
		bound.at({{0u}}) = 32u;
		bound.at({{1u}}) = 16u;
	}

	auto ind_frac = lbm::position_to_index_and_fraction_bounded(pos,bound);
	auto& ind = ind_frac.template get<0u>();
	for(uint64_t i = 0u; i < 2u; ++i){
		std::cout<<ind.at({{i}}).get()<<" ";
	}
	std::cout<<std::endl;
	auto& frac = ind_frac.template get<1u>();
	for(uint64_t i = 0u; i < 2u; ++i){
		std::cout<<frac.at({{i}}).get()<<" ";
	}
	std::cout<<std::endl;

	SAW_EXPECT(true, "Default true check");
}

}
