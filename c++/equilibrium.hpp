#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {
template<typename T, typename Descriptor>
saw::data<sch::FixedArray<T, Descriptor::Q>> equilibrium(saw::data<sch::T> rho, saw::data<sch::FixedArray<T,Descriptor::D>> vel){
	using dfi = df_info<T, Descriptor>;

	saw::data<sch::FixedArray<T,Descriptor::Q>> eq;
	saw::data<T> vel_vel{};
	for(uint64_t j = 0u; j < Descriptor::D; ++j){
		vel_vel = vel_vel + vel.at(j) * vel.at(j);
	}

	for(uint64_t i = 0u; i < eq.get_dim_size<0u>(); ++i){
		saw::data<T> vel_c{};
		for(uint64_t j = 0u; j < Descriptor::D; ++j){
			vel_c = vel_c + (vel.at(j) * {dfi::directions[i][j]});
		}

		auto vel_c_cs2 = vel_c * {dfi::inv_cs2};

		eq.at(i).set(
			dfi::weights[i] * rho.get() * 
			(
			 1.0
			 + vel_c_cs2
			 - dfi::inv_cs2 * 0.5 * vel_vel.get()
			 + vel_c_cs2 * vel_c_cs2 * 0.5
			)
		);

		return eq;
	}
}
}
}
