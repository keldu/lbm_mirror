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
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
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

	iterator<Desc::D>::apply(
		[&](const auto& index){
			auto& info = info_f.at(index);
			saw::data<sch::Scalar<T>> radius_p;
			radius_p.at({}).set(conv.meter_si_to_lbm({{0.05}}).handle().get());
			saw::data<sch::Scalar<T>> dense_p;
			dense_p.at({}).set(1);

			saw::data<sch::Vector<T,Desc::D>> middle,ind_vec;
			middle.at({{0u}}) = conv.meter_si_to_lbm({{0.16}}).handle();
			middle.at({{1u}}) = conv.meter_si_to_lbm({{0.2}}).handle();

			for(uint64_t i{0u}; i < Desc::D; ++i){
				ind_vec.at({{i}}) = index.at({i}).template cast_to<T>();
			}

			auto dist = ind_vec - middle;
			auto dist_2 = saw::math::dot(dist,dist);

			if(dist_2.at({}) < radius_p.at({}) * radius_p.at({})){
				info_f.at(index) = 1u;
			}
		},
		{},// 0-index
		df_f.get_dims()
	);
	return saw::make_void();
}

}
}
