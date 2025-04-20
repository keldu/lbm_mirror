#pragma once

#include "descriptor.hpp"
#include "converter.hpp"

#include <forstio/codec/unit/unit_print.hpp>
#include <iostream>

namespace kel {
namespace lbm {
template<typename T>
void print_lbm_meta(const converter<T>& conv){
	std::cout
		<<"[LBM Meta]\n"
		<<"==========\n"
		<<"\n"
		<<"Δx: "<<conv.delta_x()<<"\n"
		<<"Δt: "<<conv.delta_t()<<"\n"
		<<""
	;
}
}
}
