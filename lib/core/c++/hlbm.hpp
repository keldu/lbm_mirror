#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct HLBM {};
}

/**
 * HLBM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::HLBM, Encode> final {
private:
	typename saw::native_data_type<T>::type relaxation_;
	saw::data<T> frequency_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto& porosity_f = macros.template get<"porosity">();
		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();

		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Descriptor::D>> vel = vel_f.at(index);

		compute_rho_u<T,Descriptor::D>(dfs_old_f.at(index), rho, vel);

		auto& porosity = porosity_f.at(index);
		auto flip_porosity = saw::data<sch::Scalar<T>>{1.0} - porosity;

		auto& N = particle_N_f.at(index);
		auto& D = particle_D_f.at(index);
		// Convex combination of velocities
		vel = vel * flip_porosity + N * porosity / D;
		// Equilibrium
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}

		porosity.at({}) = 1.0;

	}
};

}
}
