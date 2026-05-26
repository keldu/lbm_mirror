#pragma once

#include <forstio/codec/math.hpp>
#include <forstio/codec/data_math.hpp>
#include <forstio/codec/data.hpp>

#include "../iterator.hpp"

namespace kel {
namespace lbm {
namespace coll {
struct Spheroid{};
}
namespace sch {
using namespace saw::schema;

namespace impl {
template<typename T,uint64_t D>
struct rotation_type_helper;

template<typename T>
struct rotation_type_helper<T,2u> {
	using Schema = Scalar<T>;
};

template<typename T>
struct rotation_type_helper<T,3u> {
	using Schema = Vector<T,3u>;
};
}

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<Vector<T,D>, "position">,
	Member<Vector<T,D>, "position_old">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation_old">,

	Member<Vector<T,D>, "acceleration">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "angular_acceleration">
>;

template<typename T, typename saw::native_data_type<T>::type radius = 1.0f>
using ParticleCollisionSpheroid = Struct<
>;

template<typename T, uint64_t D>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">
	// Problem is that dynamic data would two layered
	// Member<Array<Float64,D>, "mask">, 
>;

template<typename T, uint64_t D, typename CollisionType = ParticleCollisionSpheroid<T,2.0f>>
using ParticleGroup = Struct<
	Member<Array<T,D>, "mask">,
	Member<FixedArray<Scalar<T>,1u>, "density">,
	Member<Vector<T,D>, "center_of_mass">,
	Member<Scalar<T>, "total_mass">,
	Member<Array<Particle<T,D>>, "particles">
>;
}

template<typename T, uint64_t D, typename saw::native_data_type<T>::type radius>
saw::data<sch::ParticleGroup<T,D, sch::ParticleCollisionSpheroid<T,radius>>> create_spheroid_particle_group(
		saw::data<sch::Scalar<T>> density_p,
		const saw::data<sch::UInt64>& mask_resolution
){
	saw::data<sch::ParticleGroup<T,D,sch::ParticleCollisionSpheroid<T,2.0f>>> part;

	auto& mask = part.template get<"mask">();
	auto& density = part.template get<"density">().at({{0u}});

	auto& total_mass = part.template get<"total_mass">();
	// Paranoia
	total_mass.at({}) = {};

	static_assert(D >= 1u and D <= 3u, "Dimensions only supported for Dim 1,2 & 3.");
	density = density_p;

	saw::data<sch::FixedArray<sch::UInt64,D>> mask_dims;
	for(uint64_t i = 0u; i < D; ++i){
		mask_dims.at({i}) = mask_resolution;
	}
	saw::data<sch::Scalar<T>> mask_step;
	saw::data<T> rad_d{radius};
	saw::data<T> dia_d = rad_d * 2;
	mask_step.at({}) = dia_d / mask_resolution.template cast_to<T>();

	mask = {mask_dims};

	auto& com = part.template get<"center_of_mass">();
	// Paranoia
	for(uint64_t i = 0u; i < D; ++i){
		com.at({{i}}) = {};
	}
	saw::data<sch::UInt64> ele_ctr{0u};

	// Radius ^ 2
	saw::data<sch::Scalar<T>> rad_2_d;
	rad_2_d.at({}) = rad_d * rad_d;

	saw::data<sch::Vector<T,D>> center;
	for(uint64_t i = 0u; i < D; ++i){
		center.at({{i}}).set(radius);
	}

	iterator<D>::apply([&](const auto& index){
		++ele_ctr;

		saw::data<sch::Vector<T,D>> offset_index = saw::math::vectorize_data(index).template cast_to<T>() - center;

		

		auto& dpi = mask.at(index);

		for(uint64_t i = 0u; i < D; ++i){
			com.at({{i}}) = com.at({{i}}) + index.at({i}).template cast_to<T>() * dpi.at({});
		}

		total_mass = total_mass + dpi;

	},{},mask_dims);

	return part;
}

/*
template<typename T, uint64_t D>
saw::data<sch::Particle<T,D, sch::ParticleCollisionSpheroid<T>>> create_spheroid_particle(
		saw::data<sch::Vector<T,D>> pos_p,
		saw::data<sch::Vector<T,D>> vec_p,
		saw::data<sch::Vector<T,D>> acc_p,
		saw::data<sch::Vector<T,D>> rot_pos_p,
		saw::data<sch::Vector<T,D>> rot_vel_p,
		saw::data<sch::Vector<T,D>> rot_acc_p,
		saw::data<sch::Scalar<T>> rad_p,
		saw::data<sch::Scalar<T>> density_p,
		saw::data<sch::Scalar<T>> dt
	){

	saw::data<sch::Particle<T,D>> part;
	auto& body = part.template get<"rigid_body">();
	auto& mass = part.template get<"mass">();
	
	auto& pos = body.template get<"position">();
	auto& pos_old = body.template get<"position_old">();
	auto& acc = body.template get<"acceleration">();

	auto& rot = body.template get<"rotation">();
	auto& rot_old = body.template get<"rotation_old">();

	auto& coll = part.template get<"collision">();
	auto& rad = coll.template get<"radius">();

	pos = pos_p;
	pos_old = pos - vec_p * dt;
	acc = acc_p;
	rad = rad_p;

	if constexpr ( D == 1u){
		saw::data<sch::Scalar<T>> c;
		c.at({}).set(2.0);
		mass = rad_p * c * density_p;
	} else if constexpr ( D == 2u){
		saw::data<sch::Scalar<T>> pi;
		pi.at({}).set(3.14159);
		mass = rad_p * rad_p * pi * density_p;
	} else if constexpr ( D == 3u ){
		saw::data<sch::Scalar<T>> c;
		c.at({}).set(3.14159 * 4.0 / 3.0);
		mass = rad_p * rad_p * rad_p * c * density_p;
	} else {
		static_assert(D == 0u or D > 3u, "Dimensions only supported for Dim 1,2 & 3.");
	}

	return part;
}
*/

template<typename T,uint64_t D>
constexpr auto verlet_step_lambda = [](saw::data<sch::Particle<T,D>>& particle, saw::data<sch::Scalar<T>> time_step_delta){
	auto& body = particle.template get<"rigid_body">();

	auto& pos = body.template get<"position">();
	auto& pos_old = body.template get<"position_old">();

	auto& pos_acc = body.template get<"acceleration">();
	
	auto& rot = body.template get<"rotation">();
	auto& rot_old = body.template get<"rotation_old">();

	auto& rot_acc = body.template get<"angular_acceleration">();

	auto tsd_squared = time_step_delta * time_step_delta;

	saw::data<sch::Vector<T,D>> pos_new;
	// Actual step
	saw::data<sch::Scalar<T>> two;
	two.at({}).set(2.0);
	pos_new = pos * two - pos_old + pos_acc * tsd_squared;

	// Angular
	saw::data<typename sch::impl::rotation_type_helper<T,D>::Schema> rot_new;
	rot_new = rot * two - rot_old + rot_acc * tsd_squared;

	// Swap - Could be std::swap?
	pos_old = pos;
	pos = pos_new;

	rot_old = rot;
	rot = rot_new;
};

template<typename T, uint64_t D>
constexpr auto handle_collision = [](
		saw::data<sch::Particle<T,D>>& left, const saw::data<sch::Scalar<T>>& mass_l,
		saw::data<sch::Particle<T,D>>& right, const saw::data<sch::Scalar<T>>& mass_r,
		saw::data<sch::Vector<T,D>> unit_pos_rel, saw::data<sch::Vector<T,D>> vel_rel,
		saw::data<sch::Scalar<T>> d_t
){
	auto& rb_l = left.template get<"rigid_body">();
	auto& pos_l = rb_l.template get<"position">();
	auto& pos_old_l = rb_l.template get<"position_old">();
	auto vel_l = (pos_l-pos_old_l)/d_t;

	auto& rb_r = right.template get<"rigid_body">();
	auto& pos_r = rb_r.template get<"position">();
	auto& pos_old_r = rb_r.template get<"position_old">();
	auto vel_r = (pos_r-pos_old_r)/d_t;

	auto vel_pos_rel_dot = saw::math::dot(unit_pos_rel,vel_rel);

  if( vel_pos_rel_dot.at({{0u}}).get() < 0.0 ){
		pos_l = pos_l + vel_rel * unit_pos_rel * d_t;
		pos_r = pos_r - vel_rel * unit_pos_rel * d_t;
	}
};


template<typename T, uint64_t D, typename Collision>
constexpr auto broadphase_collision_distance_squared = [](
	saw::data<sch::Particle<T,D>>& left,
	const saw::data<Collision>& coll_l,
	saw::data<sch::Particle<T,D>>& right,
	const saw::data<Collision>& coll_r
) -> std::pair<bool,saw::data<sch::Scalar<T>>>{

	auto rad_l = coll_l.template get<"radius">();
	auto rad_r = coll_r.template get<"radius">();

	auto& rb_l = left.template get<"rigid_body">();
	auto& rb_r = right.template get<"rigid_body">();

	auto& pos_l = rb_l.template get<"position">();
	auto& pos_r = rb_r.template get<"position">();

	auto pos_dist = pos_l - pos_r;

	auto norm_2 = saw::math::dot(pos_dist,pos_dist);

	saw::data<sch::Scalar<T>> two;
	two.at({}) = 2.0;
	auto rad_ab_2 = rad_l * rad_l + rad_r * rad_r + rad_r * rad_l * two;

	return std::make_pair((norm_2.at({}).get() < rad_ab_2.at({}).get()), norm_2);
};
/**
*
*
*/
template<typename T, uint64_t D, typename Collision>
constexpr auto broadphase_collision_check = [](
	saw::data<sch::Particle<T,D>>& left,
	const saw::data<Collision>& coll_l,
	saw::data<sch::Particle<T,D>>& right,
	const saw::data<Collision>& coll_r
) -> bool{
	return broadphase_collision_distance_squared<T,D,Collision>(left,coll_l,right,coll_r).first;
};



namespace impl {

}

}
}
