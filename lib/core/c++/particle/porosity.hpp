#pragma once

#include "particle.hpp"
#include "../math/n_closest.hpp"

namespace kel {
namespace lbm {
template<typename T, uint64_t D, typename Coll>
class particle_porosity {
public:
	saw::data<sch::Scalar<T>> calculate(const saw::data<>& part_group, uint64_t p_i, const saw::data<sch::Vector<T,D>>& lbm_pos){
		auto& mask = part_group.template get<"mask">();

		auto& particles = part_group.template get<"particles">();
		auto& part_i = particles.at({p_i});

		auto& part_i_rb = part_i.template get<"rigid_body">();
		auto& pirb = part_i_rb.template get<"position">();

		auto& dist = lbm_pos = lbm_pos - pirb;

		// index 0 is at 

		return {};
	}
};


template<typename T, uint64_t D, typename saw::native_data_type<T>::type radius>
class particle_porosity<T, D, coll::ParticleCollisionSpheroid<T,radius>> final {
public:
	saw::data<sch::Scalar<T>> calculate(const saw::data<sch::Particle>&, uint64_t i, const saw::data<sch::Vector<T,D>>& lbm_pos){
		saw::data<sch::Scalar<T>> por;
		por.at({});

		saw::data<sch::Scalar<T>> dps_2;
		for(uint64_t i{0u}; i < D; ++i){
			auto& dps_i = lbm_pos.at({{i}});
			dps_2.at({}) = dps_i * dps_i;
		}

		saw::data<sch::Scalar<T>> rad_2;
		rad_2.at({}).set(radius*radius);

		saw::data<sch::Scalar<T>> inside;
		if(dps_2.at({}).get() < rad_2.at({}).get()){
			inside.at({}).set(1);
		}
		return inside;
	}
};

}
}
