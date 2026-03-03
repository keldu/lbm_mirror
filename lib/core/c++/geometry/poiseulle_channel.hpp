#pragma once

#include "../geometry.hpp"

namespace kel {
namespace lbm {
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

}
}
