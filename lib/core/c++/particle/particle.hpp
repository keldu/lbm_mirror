#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/data_math.hpp>
#include <forstio/codec/math.hpp>

namespace kel {
namespace lbm {
namespace coll {
struct Spheroid{};
}
namespace sch {
using namespace saw::schema;

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<Vector<T,D>, "position">,
	Member<Vector<T,D>, "position_old">,
	Member<T, "rotation">,
	Member<T, "rotation_old">,

	Member<Vector<T,D>, "acceleration">,
	Member<T, "rotational_acceleration">
>;

template<typename T>
using ParticleCollisionSpheroid = Struct<
	Member<T, "radius">
>;

template<typename T, uint64_t D, typename CollisionType = ParticleCollisionSpheroid<T>>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">,
	Member<CollisionType, "collision">,
	Member<T, "mass">
>;
}

template<typename T, uint64_t D>
saw::data<sch::Particle<T,D>, sch::ParticleCollisionSpheroid<T>> create_spheroid_particle(
		saw::data<sch::Vector<T,D>> pos_p,
		saw::data<sch::Vector<T,D>> vec_p,
		saw::data<sch::Vector<T,D>> acc_p,
		saw::data<sch::Vector<T,D>> rot_pos_p,
		saw::data<sch::Vector<T,D>> rot_vel_p,
		saw::data<sch::Vector<T,D>> rot_acc_p,
		saw::data<T> rad_p,
		saw::data<T> density_p
	){

	saw::data<sch::Particle<T,D>> part;
	auto& body = part.template get<"rigid_body">();
	auto& pos = body.template get<"position">();

	auto& pos_old = body.template get<"position_old">();
	auto& acc = body.template get<"acceleration">();

	auto& rot = body.template get<"rotation">();
	auto& rot_old = body.template get<"rotation_old">();

	auto& coll = part.template get<"collision">();
	auto& rad = coll.template get<"radius">();

	pos = pos_p;
	pos_old = pos - vec_p;
	acc = acc_p;
	rad = rad_p;

	return part;
}

template<typename T,uint64_t D>
constexpr auto verlet_step_lambda = [](saw::data<sch::Particle<T,D>>& particle, saw::data<T> time_step_delta){
		auto& body = particle.template get<"rigid_body">();

		auto& pos = body.template get<"position">();
		auto& pos_old = body.template get<"position_old">();

		auto& rot = body.template get<"rotation">();
		auto& acc = body.template get<"acceleration">();

		auto tsd_squared = time_step_delta * time_step_delta;

		saw::data<sch::Vector<T,D>> pos_new;
		// Actual step
		for(uint64_t i = 0u; i < D; ++i){
			pos_new.at({{i}}) = saw::data<T>{2.0} * pos.at({{i}}) - pos_old.at({{i}}) + acc.at({{i}}) * tsd_squared;
		}

		pos_old = pos;
		pos = pos_new;
};

template<typename T, uint64_t D>
constexpr auto handle_collision = [](saw::data<sch::Particle<T,D>>& left, saw::data<sch::Particle<T,D>>& right, saw::data<sch::Vector<T,D>> unit_pos_rel, saw::data<sch::Vector<T,D>> vel_rel)
{
	auto& rb_l = left.template get<"rigid_body">();
	auto& pos_l = rb_l.template get<"position">();
	auto& pos_old_l = rb_l.template get<"position_old">();
	auto vel_l = pos_l - pos_old_l;
	auto& mass_l = left.template get<"mass">();

	auto& rb_r = right.template get<"rigid_body">();
	auto& pos_r = rb_r.template get<"position">();
	auto& pos_old_r = rb_r.template get<"position_old">();
	auto vel_r = pos_r - pos_old_r;
	auto& mass_r = left.template get<"mass">();

	/**
	 * E to 0
	 */

	constexpr saw::data<sch::Scalar<T>> elasticity{0u};
	constexpr saw::data<sch::Scalar<T>> one{1.0};

	saw::data<sch::Scalar<T>> j = (e - one) / ((one / mass_l) + (one / mass_r));

	vel_l = vel_l + vel_rel * j;
	vel_r = vel_r - vel_rel * j;

};


template<typename T, uint64_t D>
constexpr auto broadphase_collision_distance_squared = [](saw::data<sch::Particle<T,D>>& left, saw::data<sch::Particle<T,D>>& right) -> std::pair<bool,saw::data<sch::Scalar<T>>>{
	auto rad_l = left.template get<"collision">().template get<"radius">();
	auto rad_r = right.template get<"collision">().template get<"radius">();

	auto& rb_l = left.template get<"rigid_body">();
	auto& rb_r = right.template get<"rigid_body">();

	auto& pos_l = rb_l.template get<"position">();
	auto& pos_r = rb_r.template get<"position">();

	auto pos_dist = pos_l - pos_r;

	auto norm_2 = saw::math::dot(pos_dist,pos_dist);

	auto rad_ab_2 = rad_l * rad_l + rad_r * rad_r + rad_r * rad_l * static_cast<saw::native_data_type<T>::type>(2);

	return std::make_pair((norm_2.at({}).get() < rad_ab_2.get()), norm_2);
};
/**
*
*
*/
template<typename T, uint64_t D>
constexpr auto broadphase_collision_check = [](saw::data<sch::Particle<T,D>>& left, saw::data<sch::Particle<T,D>>& right) -> bool{
	return broadphase_collision_distance_squared<T,D>(left,right).first;
};
}
}
