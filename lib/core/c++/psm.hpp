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
 * HLBM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PSM> {
private:
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& porous = field.template get<"porosity">();

				
	}
};

}
}
