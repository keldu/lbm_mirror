#include <forstio/test/suite.hpp>

#include "../c++/equilibrium.hpp"


namespace {

template<typename Descriptor>
void check_equilibrium(){
	using namespace kel;

	using dfi = lbm::df_info<lbm::sch::Float64,Descriptor>;

	saw::data<lbm::sch::Scalar<lbm::sch::Float64>> rho;
	rho.at({}).set(1.0);
	saw::data<lbm::sch::Vector<lbm::sch::Float64,Descriptor::D>> vel;
	for(saw::data<lbm::sch::UInt64> i{0u}; i.get() < Descriptor::D; ++i){
		vel.at({{i}}) = {0.0};
	}
	auto eq = lbm::equilibrium<lbm::sch::Float64,Descriptor>(rho,vel);

	for(saw::data<lbm::sch::UInt64> i{0u}; i.get() < Descriptor::Q; ++i){
		SAW_EXPECT(eq.at(i).get() == dfi::weights[i.get()], std::string{"No momentum and normalized rho should be exactly the weights: "} + std::to_string(eq.at(i).get()) + std::string{" "} + std::to_string(dfi::weights[i.get()]));
	}
}

SAW_TEST("Equilibrium at rest D1Q3"){
	using namespace kel;
	check_equilibrium<lbm::sch::Descriptor<1,3>>();
}

SAW_TEST("Equilibrium at rest D2Q5"){
	using namespace kel;
	check_equilibrium<lbm::sch::Descriptor<2,5>>();
}

SAW_TEST("Equilibrium at rest D2Q9"){
	using namespace kel;
	check_equilibrium<lbm::sch::Descriptor<2,9>>();
}
}
