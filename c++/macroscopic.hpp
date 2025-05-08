#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {
/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename T, typename Desc>
void compute_rho_u (
		const saw::data<sch::Cell<T, Desc, 0, 0, 1>>& dfs,
		typename saw::native_data_type<T>::type& rho,
		std::array<typename saw::native_data_type<T>::type, 2>& vel
	)
{
	using dfi = df_info<T, Desc>;

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

/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename T, typename Desc>
void compute_rho_u (
		const saw::data<sch::Cell<T, Desc, 0, 0, 1>>& dfs,
		saw::ref<saw::data<T>> rho,
		saw::ref<saw::data<sch::FixedArray<T,Desc::D>>> vel
	)
{
	using dfi = df_info<T, Desc>;

	rho().set(0);
	for(uint64_t i = 0; i < Desc::D; ++i){
		vel().at({i}).set(0);
	}

	for(size_t j = 0; j < Desc::Q; ++j){
		rho = rho + dfs(j);
		for(size_t i = 0; i < Desc::D; ++i){
			vel[i] = vel[i] + saw::data<T>{dfi::directions[j][i]} * dfs(j);
		}
	}

	for(size_t i = 0; i < Desc::D; ++i){
		vel[i] = vel[i] / rho;
	}
}
}
}
