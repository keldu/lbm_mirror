#pragma once

#include "particle.hpp"
#include "../math/n_closest.hpp"

namespace kel {
namespace lbm {
template<typename T, uint64_t D, typename Coll>
class particle_porosity {
public:

	
	saw::data<sch::Scalar<T>> calculate(const saw::data<sch::ParticleGroup<T,D,Coll>>& part_group, uint64_t p_i, const saw::data<sch::Vector<T,D>>& lbm_pos){
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


template<typename T, uint64_t D, typename saw::native_data_type<T>::type radius, typename saw::native_data_type<T>::type eps>
class particle_porosity<T, D, coll::ParticleCollisionSpheroid<T,radius, eps>> final {
public:
	saw::data<sch::Scalar<T>> calculate(const saw::data<sch::ParticleGroup<T,D,sch::ParticleCollisionSpheroid<T,radius,eps> > >& part_group, uint64_t i, const saw::data<sch::Vector<T,D>>& lbm_pos){
		saw::data<sch::Scalar<T>> por;
		por.at({});

		auto& parts = part_group.template get<"particles">();
		auto& pi = parts.at({i});
		auto& pi_rb = pi.template get<"rigid_body">();

		auto& pi_rb_pos = pi_rb.template get<"position">();

		// Basically the queried position minus the center of the particle
		auto dist = lbm_pos - pi_rb_pos;

		saw::data<sch::Scalar<T>> dist_len;
		for(uint64_t i{0u}; i < D; ++i){
			auto& d_i = dist.at({{i}});
			dist_len.at({}) = d_i * d_i;
		}
		dist_len.at({}).set(std::sqrt(dist_len.at({}).get());

		saw::data<sch::Scalar<T>> rad_d{radius};
		saw::data<sch::Scalar<T>> eps_half{eps *0.5};
		
		saw::data<sch::Scalar<T>> rad_d_eps_p = rad_d + eps_half;
		saw::data<sch::Scalar<T>> rad_d_eps_n = rad_d - eps_half;

		// saw::data<sch::Scalar<T>> rad_d_eps_p_2 = rad_d_eps_p * rad_d_eps_p;
		// saw::data<sch::Scalar<T>> rad_d_eps_n_2 = rad_d_eps_n * rad_d_eps_n;
		// saw::data<sch::Scalar<T>> rad_2;
		// rad_2 = rad_d * rad_d;

		saw::data<sch::Scalar<T>> inside;
		if(dist_len.at({}) <= rad_d_eps_n.at({})){
			inside.at({}).set(1);
			return inside;
		}
		if(dist_len.at({}) > rad_d_eps_p.at({})){
			inside.at({}).set(0);
			return inside
		}

		/*
		 * cos^2 ( ||x-X(t)||_2 - (R-eps/2) )
		 */
		{
			typename saw::native_data_type<T>::type inner = (std::pi / ( 2.0 * eps )) * (dist_len - rad_d_eps_n).at({}).get();
			auto cos_inner = std::cos(inner);
			auto cos_i_2 = cos_inner * cos_inner;
			inside.at({}).set(cos_i_2);
		}
		return inside;
	}
};

}
}
