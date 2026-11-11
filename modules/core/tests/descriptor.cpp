#include <forstio/test/suite.hpp>

#include "../c++/descriptor.hpp"

#include <cmath>


namespace {
/**
 * The lattice moment conditions:
 *
 *   sum_i w_i                 = 1
 *   sum_i w_i e_ia            = 0
 *   sum_i w_i e_ia e_ib       = cs^2 * delta_ab
 *
 * A weight table that violates these still compiles and still passes an
 * opposite-index check, but it makes the equilibrium return the wrong mass and
 * momentum, and the only symptom is a simulation that diverges within a few
 * steps. D3Q27 carried exactly that bug: its six face weights read 1/36 rather
 * than 2/27, so the weights summed to 13/18.
 *
 * The second moment is the useful one -- it pins the weights per shell, where
 * the plain sum only pins them in aggregate.
 */
template<typename Descriptor>
void check_weight_moments(){
	using namespace kel;

	using dfi = lbm::df_info<lbm::sch::Float64, Descriptor>;

	constexpr double tol = 1e-12;

	double sum_w = 0.0;
	for(uint64_t k = 0u; k < Descriptor::Q; ++k){
		sum_w += dfi::weights[k];
	}
	SAW_EXPECT(std::abs(sum_w - 1.0) < tol, "Weights have to sum to 1");

	for(uint64_t a = 0u; a < Descriptor::D; ++a){
		double first = 0.0;
		for(uint64_t k = 0u; k < Descriptor::Q; ++k){
			first += dfi::weights[k] * static_cast<double>(dfi::directions[k][a]);
		}
		SAW_EXPECT(std::abs(first) < tol, "First moment has to vanish");
	}

	for(uint64_t a = 0u; a < Descriptor::D; ++a){
		for(uint64_t b = 0u; b < Descriptor::D; ++b){
			double second = 0.0;
			for(uint64_t k = 0u; k < Descriptor::Q; ++k){
				second +=
					dfi::weights[k]
					* static_cast<double>(dfi::directions[k][a])
					* static_cast<double>(dfi::directions[k][b]);
			}
			const double expected = (a == b) ? static_cast<double>(dfi::cs2) : 0.0;
			SAW_EXPECT(std::abs(second - expected) < tol, "Second moment has to be cs^2 delta_ab");
		}
	}
}

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

SAW_TEST("Opposites and Dirs D1Q3"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<1,3>>();
}

SAW_TEST("Opposites and Dirs D2Q5"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<2,5>>();
}
SAW_TEST("Opposites and Dirs D2Q9"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<2,9>>();
}
SAW_TEST("Opposites and Dirs D3Q19"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<3,19>>();
}
SAW_TEST("Opposites and Dirs D3Q27"){
	using namespace kel;
	check_opposite_dirs<lbm::sch::Descriptor<3,27>>();
}

/**
 * D2Q4 is deliberately absent: it is experimental and its weights sum to 5/6,
 * so it would fail these outright.
 */
SAW_TEST("Weight moments D1Q3"){
	using namespace kel;
	check_weight_moments<lbm::sch::Descriptor<1,3>>();
}
SAW_TEST("Weight moments D2Q5"){
	using namespace kel;
	check_weight_moments<lbm::sch::Descriptor<2,5>>();
}
SAW_TEST("Weight moments D2Q9"){
	using namespace kel;
	check_weight_moments<lbm::sch::Descriptor<2,9>>();
}
SAW_TEST("Weight moments D3Q19"){
	using namespace kel;
	check_weight_moments<lbm::sch::Descriptor<3,19>>();
}
SAW_TEST("Weight moments D3Q27"){
	using namespace kel;
	check_weight_moments<lbm::sch::Descriptor<3,27>>();
}
}
