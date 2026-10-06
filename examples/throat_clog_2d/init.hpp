#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, typename Desc, typename Coll>
saw::error_or<void> init(
		const converter<T>& conv,
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros,
		saw::data<sch::ParticleSpheroidGroup<T,Desc>>& particles
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

	auto dims = info_f.get_dims();
	auto width = dims.at({{0u}});
	auto height = dims.at({{1u}});
	// 
	auto w = width.template cast_to<T>();
	auto h = height.template cast_to<T>();
	auto w2 = w * 0.5;
	auto h4 = h * 0.25;

	saw::data<T> e{0.1};
	saw::data<T> c = w * (saw::data<T>{1.0} - e) * 0.5;

	saw::data<T> d{0.2};
	auto b = h * (saw::data<T>{1.0} - d) * 0.5;

	auto p_r = (c*c + b*b) / (b*2);
	auto p_r_2 = p_r * p_r;
	std::cout<<"p²_r: "<<p_r_2.get()<<std::endl; 
	auto p_i = p_r - b;

	// Circle Top
	saw::data<sch::FixedArray<sch::Int64,Desc::D>> top_pos;
	{
		top_pos.at({0u}) = w2.get();
		top_pos.at({1u}) = (h + p_i).get();
	}

	// Circle Bot
	saw::data<sch::FixedArray<sch::Int64,Desc::D>> bot_pos;
	{
		bot_pos.at({0u}) = w2.get();
		bot_pos.at({1u}) = -p_i.get();
	}

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& info = info_f.at(index);

			auto tp_diff = top_pos;
			auto bp_diff = bot_pos;
			for(uint64_t i{0u}; i < Desc::D; ++i){
				tp_diff.at(i) = top_pos.at(i) - index.at(i).template cast_to<sch::Int64>();
				bp_diff.at(i) = bot_pos.at(i) - index.at(i).template cast_to<sch::Int64>();
			}

			auto tpd_v = saw::math::vectorize_data(tp_diff);
			auto tpd_dot = saw::math::dot(tpd_v,tpd_v);
			
			auto bpd_v = saw::math::vectorize_data(bp_diff);
			auto bpd_dot = saw::math::dot(bpd_v,bpd_v);

			if(
				(tpd_dot.at({}).template cast_to<T>() <= p_r_2 ) or
				(bpd_dot.at({}).template cast_to<T>() <= p_r_2 )
			){
				info.set({1u});
			}
		},
		{},
		info_f.get_dims(),
		{}
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
		saw::data<sch::Scalar<T>> radius_p;
		radius_p.at({}).set(conv.meter_si_to_lbm({{0.05}}).handle().get());
		std::cout<<"RADIUS: "<<radius_p.at({}).get()<<std::endl;
		saw::data<sch::Scalar<T>> dense_p;
		dense_p.at({}).set(1);
		particles = create_spheroid_particle_group<T,Desc::D,1u>(radius_p, dense_p, {{64u}});
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
	}

	return saw::make_void();
}

}
}
