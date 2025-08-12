#pragma once

namespace kel {
namespace lbm {
template<typename T, typename Descriptor>
struct is_neighbour {
	template<typename CellFieldSchema>
	static bool operator()(saw::data<CellFieldSchema>& latt,  saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>& index, saw::data<sch::UInt64> range) {
		using dfi = df_info<T,Descriptor>;
	}
};
}
}
