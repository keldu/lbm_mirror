#pragma once

#include "particle.hpp"

namespace kel {
namespace lbm {
template<typename T, uint64_t D, typename PColl>
class particle_aabb final {
};

template<typename T, uint64_t D, typename saw::native_data_type<T>::type radius>
class particle_aabb<ParticleGroup<T,D,sch::ParticleCollisionSpheroid<T,radius> > > final {
public:
	using Schema = sch::ParticleGroup<T,D,sch::ParticleCollisionSpheroid<T,radius>>;

	using AABB = Struct<
		Member<sch::FixedArray<sch::UInt64,D>"a">,
		Member<sch::FixedArray<sch::UInt64,D>"b">
	>;
public:
	static constexpr saw::data<AABB> get(const saw::data<Schema>& p_grp, const saw::data<sch::FixedArray<sch::UInt64,1u>>& i, const saw::data<sch::FixedArray<sch::UInt64,D>>& meta){
		auto& parts = p_grp.template get<"particles">();

		auto& pi = parts.at(i);
		auto& pirb = pi.template get<"rigid_body">();
		auto& pirb_pos = pirb.template get<"position">();

		saw::data<AABB> aabb;
		auto& a = aabb.template get<"a">();
		auto& b = aabb.template get<"b">();

		saw::data<sch::Scalar<T>> rad_d;
		rad_d.at({}).set(radius);

		saw::data<sch::Vector<T,D>> lower;
		saw::data<sch::Vector<T,D>> upper;

		for(uint64_t i{0u}; i < D; ++i){
			lower.at({{i}}) = pirb_pos.at({{i}}) >= rad_d.at({}) ? (pirb_pos.at({{i}}) - rad_d.at({})) : saw::data<T>{0};
			a.at({i}) = lower.at({{i}}).template cast_to<sch::UInt64>();
			upper.at({{i}}) = pirb_pos.at({{i}}) + rad_d.at({});
			b.at({i}) = (upper.at({{i}})+saw::data<T>{1}).template cast_to<sch::UInt64>()
		}
		
		return aabb;
	}
};
}
}
