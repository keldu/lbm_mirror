#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {
template<typename T, typename Descriptor>
kel::lbm::data<sch::FixedArray<T, Descriptor::Q>> equilibrium(kel::lbm::data<sch::Scalar<T>> rho, kel::lbm::data<sch::Vector<T,Descriptor::D>> vel){
	using dfi = df_info<T, Descriptor>;

	kel::lbm::data<sch::FixedArray<T,Descriptor::Q>> eq;
	// Brain broken, here's an owl
	//   ^
	//  0.0
	//  / \
	//  | |
	//
	//  Velocity * Velocity meaning || vel ||_2^2 or <vel,vel>_2
	kel::lbm::data<T> vel_vel{0.0};
	for(uint64_t j = 0u; j < Descriptor::D; ++j){
		vel_vel = vel_vel + vel.at({{j}}) * vel.at({{j}});
	}

	/**
	 * Calculate equilibrium
	 */
	for(uint64_t i = 0u; i < Descriptor::Q; ++i){
		kel::lbm::data<T> vel_c{};
		for(uint64_t j = 0u; j < Descriptor::D; ++j){
			// <vel,c_i>_2
			vel_c = vel_c + (vel.at({{j}}) * kel::lbm::data<T>{static_cast<kel::lbm::native_data_type<T>::type>(dfi::directions[i][j])});
		}

		auto vel_c_cs2 = vel_c * kel::lbm::data<T>{dfi::inv_cs2};

		eq.at(i).set(
			dfi::weights[i] * rho.at({}).get() * 
			(
			 1.0
			 + vel_c_cs2.get()
			 - dfi::inv_cs2 * 0.5 * vel_vel.get()
			 + vel_c_cs2.get() * vel_c_cs2.get() * 0.5
			)
		);
	}

	return eq;
}
}
}
