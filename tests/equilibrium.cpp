#include <forstio/test/suite.hpp>

#include "../c++/equilibrium.hpp"


namespace {

template<typename Descriptor>
void check_equilibrium(){
	using namespace kel;

	using dfi = df_info<lbm::sch::Float64,Descriptor>;

	saw::data<lbm::sch::Float64> rho{1.0};
	saw::data<lbm::sch::FixedArray<lbm::sch::Float64,>> vel;
	for(saw::data<lbm::sch::UInt64> i{0u}; i < {Descriptor::D}; ++i){
		vel.at(i) = {0.0};
	}
	auto eq = lbm::equilibrium<lbm::sch::Float64,Descriptor>(rho,vel);

	for(saw::data<lbm::sch::UInt64> i{0u}; i < {Descriptor::Q}; ++i){
		SAW_CHECK(eq(i) == dfi::weights[i.get()], "No velocity and normalized rho should be exactly the weights");
	}
}

SAW_TEST("Equilibrium at rest D1Q3"){
	using namespace kel;
	check_equilibrium<lbm::sch::Descriptor<1,3>>();
}
}
