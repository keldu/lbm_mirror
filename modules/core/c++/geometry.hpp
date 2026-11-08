#pragma once

#include "component.hpp"

namespace kel {
namespace lbm {
/*
template<typename Schema>
struct geometry {
	void apply(const kel::lbm::data<Schema>& field, const kel::lbm::data<sch::FixedArray<sch::UInt64,2u>>& start, const kel::lbm::data<sch::FixedArray<sch::UInt64,2u>>& end, const kel::lbm::data<sch::UInt8>& type){

	}
};
*/

// Ghost - 0
// Wall  - 1
// Fluid - 2
// Other Stuff - 3-x
}
}
