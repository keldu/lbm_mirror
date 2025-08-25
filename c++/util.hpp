#pragma once

#include <forstio/string_literal.hpp>
#include <forstio/codec/data.hpp>

#include <iostream>

namespace kel {
namespace lbm {
/*
template<typename T, typename Descriptor, typename CellFieldSchema>
struct is_neighbour {
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
*/

/* Might be stupidly complex
class cell_info_registry final {
public:
	static uint8_t next_reg_id = 0u;

	template<string_literal T>
	static uint8_t get_registration_id() {
		static uint8_t reg_id = std::numeric_limit<uint8_t>::max();

		if(reg_id == std::numeric_limit<uint8_t>::max()){
			reg_id = next_reg_id;
			++next_reg_id;
		}

		return reg_id;
	}
};
*/

void print_progress_bar(uint32_t progress, uint32_t progress_target){
	std::cout
		<<"\r"
		<<"Progress: "
		<<((100 * progress) / progress_target)
		<<"% [";

	uint64_t i{0u};

	uint64_t max_display = 64u;
	uint64_t progress_display = (max_display * progress) / progress_target;
	for(; i < progress_display; ++i){
		std::cout<<"#";
	}
	for(; i < max_display; ++i){
		std::cout<<"-";
	}

	std::cout<<"]";
	std::cout<<std::flush;
}
}
}
