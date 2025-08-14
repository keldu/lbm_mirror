#pragma once

namespace kel {
namespace lbm {
template<typename T, typename Descriptor, typename CellFieldSchema>
struct is_neighbour {
	template<typename CellFieldSchema>
	static bool operator()(saw::data<CellFieldSchema>& latt,  saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>& index) {
		using dfi = df_info<T,Descriptor>;

		if(index.at({0u}).get() == 0u or index.at({1u}).get() == 0u){
			return false;
		}

		for(saw::data<sch::UInt64> k{0u}; k.get() < Descriptor::Q; ++k){
			// TODO
			saw::data<sch::FixedArray<sch::UInt64,2u>>
		}

		auto& cell = latt(index);
	}
};
}
}
