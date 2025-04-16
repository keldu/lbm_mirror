#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BGK {};
}

template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::BGK> {
public:
	using Component = cmpt::BGK;

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
	void apply(saw::data<sch::CellField>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, uint64_t time_step){
		bool is_even = ((time_step % 2) == 0);
		auto& cell = field.at(index);

		auto& dfs_old = is_even ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& dfs = (!is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();


	}
};
}
}
