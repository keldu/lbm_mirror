#pragma once

#include "args.hpp"
#include "schema.hpp"
#include "flatten.hpp"
#include "chunk.hpp"
#include "descriptor.hpp"
#include "boundary.hpp"
#include "converter.hpp"
#include "config.hpp"
#include "collision.hpp"
#include "component.hpp"
#include "environment.hpp"
#include "equilibrium.hpp"
#include "iterator.hpp"
#include "hlbm.hpp"
#include "macroscopic.hpp"
#include "memory.hpp"
#include "psm.hpp"
#include "stream.hpp"
#include "write_csv.hpp"
#include "write_vtk.hpp"
#include "util.hpp"

#include <forstio/codec/unit/unit_print.hpp>
#include <iostream>

namespace kel {
namespace lbm {
template<typename T, typename Desc>
void print_lbm_meta(
		const converter<T>& conv,
		const saw::data<sch::SiKinematicViscosity<T>>& kin_vis_si,
		const saw::data<sch::SiVelocity<T>>& char_vel,
		const saw::data<sch::SiMeter<T>>& char_len
){
	std::cout
		<<"[Meta]\n"
		<<"======\n"
		<<"Re:     "<<(char_vel * char_len / kin_vis_si)<<"\n"
		<<"Ma:     "<<(char_vel * saw::data<typename saw::unit_division<sch::Pure<T>, sch::SiVelocity<T>>::Schema>{std::sqrt(df_info<T,Desc>::inv_cs2)})<<"\n"
		<<"\n"
		<<"[SI]\n"
		<<"====\n"
		<<"Δx:     "<<conv.delta_x()<<"\n"
		<<"Δt:     "<<conv.delta_t()<<"\n"
		<<"Δv:     "<<conv.delta_v()<<"\n"
		<<"Δa:     "<<conv.delta_a()<<"\n"
		<<"KinVis: "<<kin_vis_si<<"\n"
		<<"CharV:  "<<char_vel<<"\n"
		<<"CharL:  "<<char_len<<"\n"
		<<"\n"
		<<"[LBM]\n"
		<<"=====\n"
		<<"KinVis: "<<conv.kinematic_viscosity_si_to_lbm(kin_vis_si)<<"\n"
		<<"τ:      "<<(saw::data<typename saw::unit_division<sch::Pure<T>, sch::LbmKinematicViscosity<T>>::Schema >{df_info<T,Desc>::inv_cs2} * conv.kinematic_viscosity_si_to_lbm(kin_vis_si) + saw::data<sch::Pure<T>>{0.5})<<"\n"
		<<std::endl
	;
}
}
}
