#pragma once

#include "common.hpp"
#include "schema.hpp"
#include "../math/n_closest.hpp"
#include "mask.hpp"

namespace kel {
namespace lbm {
namespace por {
template<typename T>
struct ParticleSpheroid {};

/**
 * Porosity read off the group's voxel mask rather than an analytic sphere.
 *
 * Shape agnostic: anything that can be voxelized participates, and the fluid
 * then sees exactly the body the contact resolution uses, because both go
 * through the same signed distance field.
 */
template<typename T>
struct ParticleVoxel {};
}

/**
 * Mask-based porosity.
 *
 * Replaces a stub that computed a distance and then returned a
 * default-constructed value, so every caller silently got zero.
 */
template<typename T, uint64_t D, uint64_t PC, typename Coll>
class particle_porosity {
public:
	template<typename GroupRef>
	static kel::lbm::data<sch::Scalar<T>> calculate(
		const GroupRef& part_group,
		uint64_t p_i,
		const kel::lbm::data<sch::Vector<T,D>>& lbm_pos,
		kel::lbm::data<sch::Scalar<T>> eps
	){
		auto& mask = part_group.template get<"mask">();
		auto& part_i = part_group.template get<"particles">().at({p_i});

		const uint64_t resolution = mask.get_dims().at({0u}).get();

		auto sdf = mask_signed_distance<T,D>(part_group,part_i,lbm_pos,resolution);
		auto solid = solid_fraction_from_sdf<T>(sdf,eps.at({}).get());

		// Convention: 1 = fluid, 0 = solid, matching the analytic path.
		kel::lbm::data<sch::Scalar<T>> por;
		por.at({}).set(1 - solid);
		return por;
	}
};

template<typename T, uint64_t D>
class particle_porosity<T, D, 1u, por::ParticleSpheroid<T>> final {
public:
	static kel::lbm::data<sch::Scalar<T>> calculate(const kel::lbm::data<sch::Vector<T,D>>& lbm_rel_dist, kel::lbm::data<sch::Scalar<T>> rad, kel::lbm::data<sch::Scalar<T>> eps){
		kel::lbm::data<sch::Scalar<T>> por;
		
		kel::lbm::data<sch::Scalar<T>> s_dist_2 = kel::lbm::math::dot(lbm_rel_dist,lbm_rel_dist);
		kel::lbm::data<sch::Scalar<T>> s_dist = kel::lbm::math::sqrt(s_dist_2);

		kel::lbm::data<sch::Scalar<T>> eps_h;
		eps_h.at({}) = eps.at({}).get() / 2;

		auto rad_low = rad - eps_h;
		auto rad_high = rad + eps_h;

		if(s_dist.at({}) <= rad_low.at({})){
			por.at({}).set(0);
			return por;
		}
		if(s_dist.at({}) >= rad_high.at({})){
			por.at({}).set(1);
			return por;
		}

		{
			typename kel::lbm::native_data_type<T>::type inner = (std::numbers::pi / ( 2 * eps.at({}).get() )) * (s_dist - rad_low).at({}).get();
			auto cos_inner = std::sin(inner);
			auto cos_i_2 = cos_inner * cos_inner;
			por.at({}).set(cos_i_2);
		}

		return por;
	}
};

template<typename T, uint64_t D, uint64_t PC>
class particle_porosity<T, D, PC, coll::Spheroid<T>> final {
public:
	static kel::lbm::data<sch::Scalar<T>> calculate(const kel::lbm::data<sch::ParticleGroup<T,D,PC,coll::Spheroid<T> > >& part_group, uint64_t i, const kel::lbm::data<sch::Vector<T,D>>& lbm_pos) {
		kel::lbm::data<sch::Scalar<T>> por;

		auto& parts = part_group.template get<"particles">();
		auto& pi = parts.at({i});
		auto& pi_rb = pi.template get<"rigid_body">();

		auto& pi_rb_pos = pi_rb.template get<"position">();

		// Basically the queried position minus the center of the particle
		auto dist = lbm_pos - pi_rb_pos;

		kel::lbm::data<sch::Scalar<T>> dist_len = kel::lbm::math::dot(dist,dist);
		dist_len.at({}).set(std::sqrt(dist_len.at({}).get()));
		auto& coll = pi.template get<"collision">();
		const kel::lbm::data<sch::Scalar<T>>& rad_d = coll.template get<"radius">().handle();
		const auto& eps = part_group.template get<"epsilon">().at({});
		// Move this somewhere
		
		kel::lbm::data<sch::Scalar<T>> eps_h;
		eps_h.at({}) = eps.at({}).get() / 2;

		kel::lbm::data<sch::Scalar<T>> rad_d_eps_p = rad_d + eps_h;
		kel::lbm::data<sch::Scalar<T>> rad_d_eps_n = rad_d - eps_h;

		// kel::lbm::data<sch::Scalar<T>> rad_d_eps_p_2 = rad_d_eps_p * rad_d_eps_p;
		// kel::lbm::data<sch::Scalar<T>> rad_d_eps_n_2 = rad_d_eps_n * rad_d_eps_n;
		// kel::lbm::data<sch::Scalar<T>> rad_2;
		// rad_2 = rad_d * rad_d;

		kel::lbm::data<sch::Scalar<T>> inside;
		if(dist_len.at({}) <= rad_d_eps_n.at({})){
			inside.at({}).set(0);
			return inside;
		}
		if(dist_len.at({}) > rad_d_eps_p.at({})){
			inside.at({}).set(1);
			return inside;
		}

		/*
		 * cos^2 ( ||x-X(t)||_2 - (R-eps/2) )
		 */
		{
			typename kel::lbm::native_data_type<T>::type inner = (std::numbers::pi / (eps.at({}).get())) * (dist_len - rad_d_eps_n).at({}).get();
			auto cos_inner = std::cos(inner);
			auto cos_i_2 = cos_inner * cos_inner;
			inside.at({}).set(cos_i_2);
		}
		return inside;
	}
};

}
}
