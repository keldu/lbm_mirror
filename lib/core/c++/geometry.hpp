#pragma once

#include "component.hpp"

namespace kel {
namespace lbm {
/*
template<typename Schema>
struct geometry {
	void apply(const saw::data<Schema>& field, const saw::data<sch::FixedArray<sch::UInt64,2u>>& start, const saw::data<sch::FixedArray<sch::UInt64,2u>>& end, const saw::data<sch::UInt8>& type){

	}
};
*/
namespace cmpt {
struct PoiseulleChannel;
}

template<typename Schema, typename Desc, typename Encode>
class component<Schema, Desc, cmpt::PoiseulleChannel, Encode> final {
private:
public:
	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema,Encode>& field, const saw::data<sch::FixedArraysch::UInt64,Desc::D>){
		auto& info_f = field.template get<"info">();
	}
};

// Ghost - 0
// Wall  - 1
// Fluid - 2
// Other Stuff - 3-x
}
}
