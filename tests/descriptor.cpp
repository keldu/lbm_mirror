#include <forstio/test/suite.hpp>

#include "../c++/descriptor.hpp"


namespace {
template<typename Descriptor>
void check_opposite_dirs(){
	using namespace kel;

	using dfi = lbm::df_info<lbm::sch::Float32, Descriptor>;

	for(uint64_t k = 0u; k < Descriptor::Q; ++k){
		auto k_inv = dfi::opposite_index[k];

		for(uint64_t i = 0u; i < Descriptor::D; ++i){
			SAW_EXPECT(dfi::directions[k][i] == (-1*dfi::directions[k_inv][i]), "Opposites are inconsistent");
		}
	}
}

SAW_TEST("Opposites and Dirs D2Q5"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<2,5>>();
}
SAW_TEST("Opposites and Dirs D2Q9"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<2,9>>();
}
}
