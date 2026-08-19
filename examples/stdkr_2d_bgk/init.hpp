#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, typename Desc, typename Coll>
saw::error_or<void> init(
		const converter<T>& conv,
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros
){
	(void) conv;

	auto& info_f = fields.template get<"info">();
	auto& porous_f = macros.template get<"porosity">();
	// Set everything as walls
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(1u);
		},
		{},
		info_f.get_dims(),
		{}
	);
	// Fluid
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(2u);
		},
		{},
		info_f.get_dims(),
		{{1u,1u}}
	);
	
	// Inflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(3u);
		},
		{{0u,0u}},
		{{1u,dim_y}},
		{{0u,1u}}
	);
	
	// Outflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(4u);
		},
		{{dim_x-1u,0u}},
		{{dim_x, dim_y}},
		{{0u,1u}}
	);
	//
	auto& df_f = fields.template get<"dfs_old">();
	auto& rho_f = macros.template get<"density">();
	auto& vel_f = macros.template get<"momentum">();
	auto& por_f = macros.template get<"porosity">();
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			por_f.at(index).at({}) = {1};
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims()
	);

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			if(info_f.at(index).get() == 2u){
				vel.at({{0u}}) = 0.0;
			}
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims(),
		{{1u,1u}}
	);

	{

	}
	{
		saw::data<sch::Scalar<T>> radius_p;
		radius_p.at({}).set(conv.meter_si_to_lbm({{0.05}}).handle().get());
		std::cout<<"RADIUS: "<<radius_p.at({}).get()<<std::endl;
		saw::data<sch::Scalar<T>> dense_p;
		dense_p.at({}).set(1);
		/*
		auto& parts = particles.template get<"particles">();

		{
			auto& p = parts.at({0u});
			auto& prb = p.template get<"rigid_body">();
			auto& p_pos = prb.template get<"position">();
			auto& p_posold = prb.template get<"position_old">();
			p_pos.at({{0u}}) = conv.meter_si_to_lbm({{0.16}}).handle();
			p_pos.at({{1u}}) = conv.meter_si_to_lbm({{0.2}}).handle();

			p_posold = p_pos;
		}
		*/
	}

	return saw::make_void();
}

}
}
