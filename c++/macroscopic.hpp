#pragma once

namespace kel {
namespace lbm {
/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename Desc>
void compute_rho_u (
		const saw::data<sch::DfCell<Desc>>& dfs,
		typename saw::native_data_type<sch::T>::type& rho,
		std::array<typename saw::native_data_type<sch::T>::type, 2>& vel
	)
{
	using dfi = df_info<sch::T, Desc>;

	rho = 0;
	std::fill(vel.begin(), vel.end(), 0);

	for(size_t j = 0; j < Desc::Q; ++j){
		rho += dfs(j).get();
		for(size_t i = 0; i < Desc::D; ++i){
			vel[i] += dfi::directions[j][i] * dfs(j).get();
		}
	}

	for(size_t i = 0; i < Desc::D; ++i){
		vel[i] /= rho;
	}
}
}
}
