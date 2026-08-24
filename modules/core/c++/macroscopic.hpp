#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {
/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename T, typename Desc>
void compute_rho_u (
		const saw::data<sch::FixedArray<T,Desc::Q>>& dfs,
		saw::ref<saw::data<sch::Scalar<T>>> rho,
		saw::ref<saw::data<sch::Vector<T,Desc::D>>> vel
	)
{
	using dfi = df_info<T, Desc>;

	rho().at({}).set(0);
	for(uint64_t i = 0; i < Desc::D; ++i){
		vel().at({{i}}).set(0);
	}

	for(size_t j = 0; j < Desc::Q; ++j){
		rho().at({}) = rho().at({}) + dfs.at({j});
		for(size_t i = 0; i < Desc::D; ++i){
			vel().at({{i}}) = vel().at({{i}}) + saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[j][i])} * dfs.at({j});
		}
	}

	// TODO - Change to momentum, but that needs checks in every impacted function
	for(size_t i = 0; i < Desc::D; ++i){
		vel().at({{i}}) = vel().at({{i}}) / rho().at({});
	}
}

/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename T, typename Desc>
void compute_rho_momentum (
		const saw::data<sch::FixedArray<T,Desc::Q>>& dfs,
		saw::ref<saw::data<sch::Scalar<T>>> rho,
		saw::ref<saw::data<sch::Vector<T,Desc::D>>> mom
	)
{
	using dfi = df_info<T, Desc>;

	rho().at({}).set(0);
	for(uint64_t i = 0; i < Desc::D; ++i){
		mom().at({{i}}).set(0);
	}

	for(size_t j = 0; j < Desc::Q; ++j){
		rho().at({}) = rho().at({}) + dfs.at({j});
		for(size_t i = 0; i < Desc::D; ++i){
			mom().at({{i}}) = mom().at({{i}}) + saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[j][i])} * dfs.at({j});
		}
	}
}
}
}
