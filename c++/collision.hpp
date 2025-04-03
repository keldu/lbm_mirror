#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BGK {};
}

template<typename T>
class component<T, cmpt::BGK> {
public:
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 0, true, true>
	>;

	static constexpr saw::string_literal name = "collision";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";
};
}
}
