#pragma once

#include "component.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct Stream {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T,Descriptor, cmpt::Stream, Encode> final {
private:
public:
	static constexpr saw::string_literal name = "streaming";
	static constexpr saw::string_literal after = "collide";
	static constexpr saw::string_literal before = "";

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step) const {

	}
};
}
}
