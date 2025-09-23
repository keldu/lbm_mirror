#pragma once

#include <forstio/string_literal.hpp>
#include <forstio/codec/data.hpp>

#include <iostream>
#include <iomanip>
#include <sys/ioctl.h>

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

void print_progress_bar(const uint32_t progress, const uint32_t progress_target){
	std::cout<<"\r";
	//	<<"Progress: "
	//	<<((100 * progress) / progress_target)
	//	<<"% [";

	const uint32_t progress_min = std::min(progress,progress_target);
	constexpr uint64_t max_perc_progress = 100u;
	uint64_t perc_prog = (max_perc_progress*progress_min) / progress_target;

	std::string_view prog_str{"Progress: "};
	// Progress string + (100 width and perc char) + ([] brackets) + space + pointer
	uint64_t i{prog_str.size() + 4u + 2u + 1u + 1u};

	std::cout<<prog_str;
	std::cout<<std::setw(3u)<<perc_prog<<"%";
	std::cout<<" ";
	std::cout<<"[";

	uint64_t max_display = []() -> uint64_t{
		struct winsize w;
		if(ioctl(STDOUT_FILENO, TIOCGWINSZ,&w) == -1){
			// Standardized Terminal size
			return 80u;
		}
		return w.ws_col;
	}();
	max_display = std::max(max_display,i) - i;
	uint64_t progress_display = (max_display * progress_min) / progress_target;

	for(i = 0u; i < progress_display; ++i){
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
