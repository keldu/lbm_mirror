#pragma once

#include "common.hpp"
#include "schema.hpp"
#include "../math/n_closest.hpp"

namespace kel {
namespace lbm {
namespace por {
template<typename T>
struct ParticleSpheroid {};
}

template<typename T, uint64_t D, typename Coll>
class particle_porosity {
public:

	static saw::data<sch::Scalar<T>> calculate(const saw::data<sch::ParticleGroup<T,D,Coll>>& part_group, uint64_t p_i, const saw::data<sch::Vector<T,D>>& lbm_pos){
		auto& mask = part_group.template get<"mask">();

		auto& particles = part_group.template get<"particles">();
		auto& part_i = particles.at({p_i});

		auto& part_i_rb = part_i.template get<"rigid_body">();
		auto& pirb = part_i_rb.template get<"position">();

		auto dist = lbm_pos - pirb;

		// index 0 is at 

		return {};
	}
};

template<typename T, uint64_t D>
class particle_porosity<T, D, por::ParticleSpheroid<T>> final {
public:
	static saw::data<sch::Scalar<T>> calculate(const saw::data<sch::Vector<T,D>>& lbm_rel_dist, saw::data<sch::Scalar<T>> rad, saw::data<sch::Scalar<T>> eps){
		saw::data<sch::Scalar<T>> por;
		
		auto s_dist_2 = saw::math::dot(lbm_rel_dist,lbm_rel_dist);
		auto s_dist = saw::math::sqrt(s_dist_2);

		saw::data<sch::Scalar<T>> eps_h;
		eps_h.at({}) = eps.at({}).get() / 2;

		auto rad_low = rad - eps_h;
		auto rad_high = rad + eps_h;

		if(s_dist.at({}) <= rad_low.at({})){
			por.at({}).set(1);
			return por;
		}
		if(s_dist.at({}) >= rad_high.at({})){
			por.at({}).set(0);
			return por;
		}

		{
			typename saw::native_data_type<T>::type inner = (std::numbers::pi / ( eps.at({}).get() )) * (s_dist - rad_low).at({}).get();
			auto cos_inner = std::cos(inner);
			auto cos_i_2 = cos_inner * cos_inner;
			por.at({}).set(cos_i_2);
		}

		return por;
	}
};


template<typename T, uint64_t D>
class particle_porosity<T, D, coll::Spheroid<T>> final {
public:
	static saw::data<sch::Scalar<T>> calculate(const saw::data<sch::ParticleGroup<T,D,coll::Spheroid<T> > >& part_group, uint64_t i, const saw::data<sch::Vector<T,D>>& lbm_pos) {
		saw::data<sch::Scalar<T>> por;

		auto& parts = part_group.template get<"particles">();
		auto& pi = parts.at({i});
		auto& pi_rb = pi.template get<"rigid_body">();

		auto& pi_rb_pos = pi_rb.template get<"position">();

		// Basically the queried position minus the center of the particle
		auto dist = lbm_pos - pi_rb_pos;

		saw::data<sch::Scalar<T>> dist_len = saw::math::dot(dist,dist);
		dist_len.at({}).set(std::sqrt(dist_len.at({}).get()));

		auto& coll = part_group.template get<"collision">();
		const saw::data<sch::Scalar<T>>& rad_d = coll.template get<"radius">();
		const auto& eps = part_group.template get<"epsilon">();
		// Move this somewhere
		
		saw::data<sch::Scalar<T>> eps_h;
		eps_h.at({}) = eps.at({}).get() / 2;

		saw::data<sch::Scalar<T>> rad_d_eps_p = rad_d + eps_h;
		saw::data<sch::Scalar<T>> rad_d_eps_n = rad_d - eps_h;

		// saw::data<sch::Scalar<T>> rad_d_eps_p_2 = rad_d_eps_p * rad_d_eps_p;
		// saw::data<sch::Scalar<T>> rad_d_eps_n_2 = rad_d_eps_n * rad_d_eps_n;
		// saw::data<sch::Scalar<T>> rad_2;
		// rad_2 = rad_d * rad_d;

		saw::data<sch::Scalar<T>> inside;
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
			typename saw::native_data_type<T>::type inner = (std::numbers::pi / (eps)) * (dist_len - rad_d_eps_n).at({}).get();
			auto cos_inner = std::cos(inner);
			auto cos_i_2 = cos_inner * cos_inner;
			inside.at({}).set(cos_i_2);
		}
		return inside;
	}
};

}
}
