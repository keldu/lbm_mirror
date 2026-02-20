#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct PSM {};
}

/**
 * PSM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PSM> {
private:
	saw::data<sch::Scalar<T>> relaxation_;
	saw::data<sch::Scalar<T>> frequency_;
public:
	component(
		saw::data<sch::Scalar<T>> relaxation__
	):
		relaxation_{relaxation__}
	{
		saw::data<sch::Scalar<T>> one;
		frequency_ = one / relaxation_;
	}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
	
		using dfi = df_info<T,Descriptor>;
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& porous = field.template get<"porosity">();

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();
	
		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		auto flip_porous = one - porous;

		auto& dfs = dfs_old_f.at(index);

		auto dfs_cpy = dfs;

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			uint64_t i_opp = dfi::opposite_index[i];
			dfs.at({i}) = dfs_cpy.at({i}) + frequency_ * (eq.at(i) - dfs_cpy.at({i})) * flip_porous + (dfs_cpy.at({i_opp}) - dfs_cpy.at({i}) ) * porous;
		}
	}
};

}
}
