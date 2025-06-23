#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BounceBack {};
}

template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::BounceBack> {
private:
	typename saw::native_data_type<T>::type relaxation_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__}
	{}

	using Component = cmpt::BounceBack;

	/**
	 * Thoughts regarding automating order setup
	 */
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "collision";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, uint64_t time_step){
		bool is_even = ((time_step % 2) == 0);
		auto& cell = field.at(index);

		auto& dfs_old = is_even ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& dfs = (!is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		using dfi = df_info<sch::T,Desc>;

		// Technically use .copy()
		auto df_cpy = dfs;

		for(uint64_t i = 1u; i < Desc::Q; ++i){
			dfs({i}) = df_cpy({dfi::opposite_index.at(i)});
		}
	}
};
}
}
