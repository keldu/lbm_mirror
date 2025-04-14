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

	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	static constexpr saw::string_literal name = "collision";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	void apply(saw::data<sch::CellField>& field){

	}
};
}
}
