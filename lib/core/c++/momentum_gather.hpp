#pragma once

#include "common.hpp"
#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
// Gather the force, Luke!
struct ForceGather {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T,Descriptor,cmpt::ForceGather, Encode> {
private:
public:
	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros,
		saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {

		using dfi = df_info<T,Descriptor>;
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& porous_f = macros.template get<"porosity">();
		auto& porous = porous_f.at(index);

		auto& force_f = macros.template get<"force">();
		auto& force = force_f.at(index);

		for(uint64_t k{0u}; k < Descriptor::D; ++k){
			force.at({{k}}).set(0);
		}

		auto& dfs = dfs_old_f.at(index);
		for(uint64_t i{0u}; i < Descriptor::Q; ++i){
			uint64_t i_opp = dfi::opposite_index[i];
			auto dfs_diff = dfs.at({i}) - dfs.at({i_opp});
			for(uint64_t k{0u}; k < Descriptor::D; ++k){
				force.at({{k}}) = force.at({{k}}) + dfs_diff * saw::data<T>{dfi::directions[i][k]};
			}
		}

		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		auto flip_porous = one - porous;
		force = force * flip_porous;
	}
};
}
}
