#pragma once

#include "common.hpp"

#include "schema.hpp"
#include "aabb.hpp"
#include "mask.hpp"
#include "particle_opa.hpp"

namespace kel {
namespace lbm {

/**
 * Builds a group of identical spheroids.
 *
 * The signature is unchanged from when radius/density/mass lived on the group:
 * the three scalars are fanned out to every particle here, so no caller moves.
 */
template<typename T, uint64_t D,uint64_t PartAmount>
kel::lbm::data<sch::ParticleGroup<T, D, PartAmount, coll::Spheroid<T>>> create_spheroid_particle_group(
		kel::lbm::data<sch::Scalar<T>> radius_p,
		kel::lbm::data<sch::Scalar<T>> density_p,
		const kel::lbm::data<sch::UInt64>& mask_resolution
){
	static_assert(D >= 1u and D <= 3u, "Dimensions only supported for Dim 1,2 & 3.");
	static_assert(PartAmount > 0u, "A group needs at least one particle");

	kel::lbm::data<sch::ParticleGroup<T,D,PartAmount,coll::Spheroid<T>>> part;

	// The group keeps the shape prototype; the particles carry their own copy.
	auto& rad_s = part.template get<"collision">().at({0u}).template get<"radius">();
	rad_s = radius_p;

	auto& mask = part.template get<"mask">();

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> mask_dims;
	for(uint64_t i = 0u; i < D; ++i){
		mask_dims.at({i}) = mask_resolution;
	}
	kel::lbm::data<T> rad_d = radius_p.at({});
	kel::lbm::data<T> dia_d = rad_d * 2;

	mask = {mask_dims};

	/**
	 * mask_step stays in LATTICE units for callers that reason in them, but
	 * the mask itself is normalized to radius 1 so one group shape serves
	 * particles of any scale. See particle/mask.hpp for the full convention.
	 */
	auto& mask_step = part.template get<"mask_step">().at({{0u}});
	mask_step.at({}) = dia_d / mask_resolution.template cast_to<T>();

	/**
	 * Fill the mask with the signed distance of a unit sphere.
	 *
	 * The loop that used to be here computed an offset and a radius-squared
	 * and discarded both without ever writing a voxel, so every mask was
	 * silently all zeros and the centre of mass came out as 0/0.
	 */
	auto& com = part.template get<"center_of_mass">().at({{0u}});
	com = fill_mask_from_sdf<T,D>(
		mask,
		mask_resolution.get(),
		[](const kel::lbm::data<sch::Vector<T,D>>& u){
			return sdf::sphere<T,D>(u);
		}
	);

	/**
	 * Analytic mass and inertia for a uniform spheroid.
	 *
	 * Taken from the closed form rather than summed off the mask: the mask is
	 * a signed distance, so a mass integral would need the ramp applied first
	 * and would only approximate what the closed form gives exactly.
	 *
	 * The 3D branch used to be empty, leaving total_mass at 0 and then
	 * dividing the centre of mass by it.
	 */
	kel::lbm::data<sch::Scalar<T>> mass_p;
	kel::lbm::data<sch::Scalar<T>> inertia_p;
	{
		constexpr typename kel::lbm::native_data_type<T>::type pi = 3.14159265358979323846;
		auto r = rad_d.get();
		auto rho = density_p.at({}).get();

		if constexpr ( D == 1u ) {
			mass_p.at({}).set(2.0 * r * rho);
			// A rod has no out-of-line rotation to speak of.
			inertia_p.at({}).set(0.0);
		} else if constexpr ( D == 2u ) {
			auto m = pi * r * r * rho;
			mass_p.at({}).set(m);
			// Disc about its centre: m r^2 / 2
			inertia_p.at({}).set(0.5 * m * r * r);
		} else {
			auto m = (4.0 / 3.0) * pi * r * r * r * rho;
			mass_p.at({}).set(m);
			// Solid sphere: 2 m r^2 / 5
			inertia_p.at({}).set(0.4 * m * r * r);
		}
	}

	auto& parts = part.template get<"particles">();
	for(uint64_t p_i = 0u; p_i < PartAmount; ++p_i){
		auto& pi = parts.at({p_i});

		pi.template get<"collision">().template get<"radius">() = radius_p;

		auto& rb = pi.template get<"rigid_body">();
		rb.template get<"mass">() = mass_p;
		rb.template get<"inertia">() = inertia_p;
		rb.template get<"density">() = density_p;
	}

	return part;
}

/*
template<typename T, uint64_t D>
kel::lbm::data<sch::Particle<T,D, sch::ParticleCollisionSpheroid<T>>> create_spheroid_particle(
		kel::lbm::data<sch::Vector<T,D>> pos_p,
		kel::lbm::data<sch::Vector<T,D>> vec_p,
		kel::lbm::data<sch::Vector<T,D>> acc_p,
		kel::lbm::data<sch::Vector<T,D>> rot_pos_p,
		kel::lbm::data<sch::Vector<T,D>> rot_vel_p,
		kel::lbm::data<sch::Vector<T,D>> rot_acc_p,
		kel::lbm::data<sch::Scalar<T>> rad_p,
		kel::lbm::data<sch::Scalar<T>> density_p,
		kel::lbm::data<sch::Scalar<T>> dt
	){

	kel::lbm::data<sch::Particle<T,D>> part;
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
		kel::lbm::data<sch::Scalar<T>> c;
		c.at({}).set(2.0);
		mass = rad_p * c * density_p;
	} else if constexpr ( D == 2u){
		kel::lbm::data<sch::Scalar<T>> pi;
		pi.at({}).set(3.14159);
		mass = rad_p * rad_p * pi * density_p;
	} else if constexpr ( D == 3u ){
		kel::lbm::data<sch::Scalar<T>> c;
		c.at({}).set(3.14159 * 4.0 / 3.0);
		mass = rad_p * rad_p * rad_p * c * density_p;
	} else {
		static_assert(D == 0u or D > 3u, "Dimensions only supported for Dim 1,2 & 3.");
	}

	return part;
}
*/

template<typename T,uint64_t D>
constexpr auto verlet_step_lambda = [](kel::lbm::data<sch::Particle<T,D>>& particle, kel::lbm::data<sch::Scalar<T>> time_step_delta){
	auto& body = particle.template get<"rigid_body">();

	auto& pos = body.template get<"position">();
	auto& pos_old = body.template get<"position_old">();

	auto& pos_acc = body.template get<"acceleration">();
	
	auto& rot = body.template get<"rotation">();
	auto& rot_old = body.template get<"rotation_old">();

	auto& rot_acc = body.template get<"angular_acceleration">();

	auto tsd_squared = time_step_delta * time_step_delta;

	kel::lbm::data<sch::Vector<T,D>> pos_new;
	// Actual step
	kel::lbm::data<sch::Scalar<T>> two;
	two.at({}).set(2.0);
	pos_new = pos * two - pos_old + pos_acc * tsd_squared;

	// Swap - Could be std::swap? No, this is device code
	pos_old = pos;
	pos = pos_new;

	// Angular
	kel::lbm::data<typename sch::impl::rotation_type_helper<T,D>::Schema> rot_new;
	rot_new = rot * two - rot_old + rot_acc * tsd_squared;
	
	rot_old = rot;
	rot = rot_new;
};

template<typename T, uint64_t D>
constexpr auto handle_collision = [](
		kel::lbm::data<sch::Particle<T,D>>& left, const kel::lbm::data<sch::Scalar<T>>& mass_l,
		kel::lbm::data<sch::Particle<T,D>>& right, const kel::lbm::data<sch::Scalar<T>>& mass_r,
		kel::lbm::data<sch::Vector<T,D>> unit_pos_rel, kel::lbm::data<sch::Vector<T,D>> vel_rel,
		kel::lbm::data<sch::Scalar<T>> d_t
){
	auto& rb_l = left.template get<"rigid_body">();
	auto& pos_l = rb_l.template get<"position">();
	auto& pos_old_l = rb_l.template get<"position_old">();
	auto vel_l = (pos_l-pos_old_l)/d_t;

	auto& rb_r = right.template get<"rigid_body">();
	auto& pos_r = rb_r.template get<"position">();
	auto& pos_old_r = rb_r.template get<"position_old">();
	auto vel_r = (pos_r-pos_old_r)/d_t;

	auto vel_pos_rel_dot = kel::lbm::math::dot(unit_pos_rel,vel_rel);

  if( vel_pos_rel_dot.at({{0u}}).get() < 0.0 ){
		pos_l = pos_l + vel_rel * unit_pos_rel * d_t;
		pos_r = pos_r - vel_rel * unit_pos_rel * d_t;
	}
};

/**
 * Bounding-sphere overlap test.
 *
 * Radius lives on the particle now, so the collision shapes no longer have to
 * be threaded through separately.
 *
 * Returns the squared centre distance alongside the verdict so a caller that
 * goes on to a narrow phase does not recompute it.
 */
template<typename T, uint64_t D, typename Collision = coll::Spheroid<T>>
constexpr auto broadphase_collision_distance_squared = []<typename PRef>(
	const PRef& left,
	const PRef& right
) -> std::pair<bool,kel::lbm::data<sch::Scalar<T>>>{

	auto rad_l = left.template get<"collision">().template get<"radius">();
	auto rad_r = right.template get<"collision">().template get<"radius">();

	auto& pos_l = left.template get<"rigid_body">().template get<"position">();
	auto& pos_r = right.template get<"rigid_body">().template get<"position">();

	auto pos_dist = pos_l - pos_r;

	auto norm_2 = kel::lbm::math::dot(pos_dist,pos_dist);

	kel::lbm::data<sch::Scalar<T>> two;
	two.at({}) = 2.0;
	// (r_l + r_r)^2
	auto rad_ab_2 = rad_l * rad_l + rad_r * rad_r + rad_r * rad_l * two;

	return std::make_pair((norm_2.at({}).get() < rad_ab_2.at({}).get()), norm_2);
};

template<typename T, uint64_t D, typename Collision = coll::Spheroid<T>>
constexpr auto broadphase_collision_check = []<typename PRef>(
	const PRef& left,
	const PRef& right
) -> bool{
	return broadphase_collision_distance_squared<T,D,Collision>(left,right).first;
};

template<typename T,uint64_t D, uint64_t PartAmount>
constexpr auto particle_dem_lambda = [](kel::lbm::data<sch::ParticleGroup<T,D,PartAmount, coll::Spheroid<T>>>& particle, kel::lbm::data<sch::Scalar<T>> time_step_delta){
	for(uint64_t i{0u}; i < PartAmount; ++i){
		auto has_collided = false;
	}
};

}
}
