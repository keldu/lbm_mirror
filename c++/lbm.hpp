#pragma once

#include "descriptor.hpp"
#include "converter.hpp"
#include "config.hpp"
#include "component.hpp"
#include "environment.hpp"
#include "equilibrium.hpp"
#include "macroscopic.hpp"
#include "write_vtk.hpp"
#include "util.hpp"

#include <forstio/codec/unit/unit_print.hpp>
#include <iostream>

namespace kel {
namespace lbm {
template<typename T, typename Desc>
void print_lbm_meta(const converter<T>& conv, const saw::data<sch::SiKinematicViscosity<T>>& kin_vis_si){
	std::cout
		<<"[LBM Meta]\n"
		<<"==========\n"
		<<"\n"
		<<"Δx:     "<<conv.delta_x()<<"\n"
		<<"Δt:     "<<conv.delta_t()<<"\n"
		<<"KinVis: "<<kin_vis_si<<"\n"
		<<"τ:      "<<(saw::data<typename saw::unit_division<sch::Pure<T>, sch::LbmKinematicViscosity<T>>::Schema >{df_info<T,Desc>::inv_cs2} * conv.kinematic_viscosity_si_to_lbm(kin_vis_si) + saw::data<sch::Pure<T>>{0.5})<<"\n"
	;
}
}
}
