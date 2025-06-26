#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BounceBack {};
}

/**
 * Full-Way BounceBack.
 */
template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::BounceBack> {
private:
public:
	component() = default;

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
	static constexpr saw::string_literal name = "full_way_bounce_back";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, uint64_t time_step){
		bool is_even = ((time_step % 2u) == 0u);

		// This is a ref
		auto& cell = field(index);

		auto& dfs_old = (is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		using dfi = df_info<T,Descriptor>;

		// Technically use .copy()
		auto df_cpy = dfs_old.copy();

		for(uint64_t i = 1u; i < Descriptor::Q; ++i){
			dfs_old({i}) = df_cpy({dfi::opposite_index.at(i)});
		}
	}
};
}
}
