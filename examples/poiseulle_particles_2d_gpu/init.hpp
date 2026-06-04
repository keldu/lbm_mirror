#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, typename Desc>
saw::error_or<void> setup_initial_conditions(
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros,
		saw::data<sch::ParticleSpheroidGroup<T,Desc>>& particles
){
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
	auto& vel_f = macros.template get<"velocity">();
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
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			saw::data<sch::Vector<T,Desc::D>> middle, ind_vec;
			middle.at({{0u}}) = dim_x * 0.25;
			middle.at({{1u}}) = dim_y * 0.5;

			ind_vec.at({{0u}}) = index.at({{0u}}).template cast_to<T>();
			ind_vec.at({{1u}}) = index.at({{1u}}).template cast_to<T>();

			auto dist = middle - ind_vec;
			auto dist_2 = saw::math::dot(dist,dist);
			if(dist_2.at({}).get() < dim_y*dim_y*0.01){
				porous_f.at(index).at({}) = 0.0;
			}
		},
		{},// 0-index
		df_f.get_dims()
	);

	{
		saw::data<sch::Scalar<T>> dense_p;
		dense_p.at({}).set(1);
		particles = create_spheroid_particle_group<T,Desc::D,2.0f>(dense_p, {{16u}});
	}

	return saw::make_void();
}

}
}
