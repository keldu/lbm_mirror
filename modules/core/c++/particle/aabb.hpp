#pragma once

#include "common.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {
template<typename PGroup>
class particle_aabb final {
	static_assert(saw::always_false<PGroup>, "Not supported");
};

template<typename T, uint64_t D, uint64_t PC>
class particle_aabb<
	sch::ParticleGroup<T,D,PC,coll::Spheroid<T>>
> final {
public:
	using Schema = sch::ParticleGroup<T,D,PC,coll::Spheroid<T>>;

	using AABB = sch::Struct<
		sch::Member<sch::FixedArray<sch::UInt64,D>,"a">,
		sch::Member<sch::FixedArray<sch::UInt64,D>,"b">
	>;
public:
	template<typename Encode>
	static constexpr saw::error_or<saw::data<AABB>> calculate(const saw::data<Schema,Encode>& p_grp, const saw::data<sch::FixedArray<sch::UInt64,1u>>& i, const saw::data<sch::FixedArray<sch::UInt64,D>>& meta){
		static_assert(PC > 0u, "Can't calculate from no particles");
		if(not (i.at({0u}).get() < PC) ){
			return saw::make_error<saw::err::critical>("Too large i in particle_aabb");
		}

		saw::data<AABB> aabb;
		auto& parts = p_grp.template get<"particles">();

		auto& pi = parts.at(i);
		auto& pirb = pi.template get<"rigid_body">();
		auto& pirb_pos = pirb.template get<"position">();

		auto& a = aabb.template get<"a">();
		auto& b = aabb.template get<"b">();

		const saw::data<sch::Scalar<T>>& rad_d = p_grp.template get<"collision">().at(i).template get<"radius">();

		saw::data<sch::Vector<T,D>> lower;
		saw::data<sch::Vector<T,D>> upper;

		for(uint64_t i{0u}; i < D; ++i){
			lower.at({{i}}) = ((pirb_pos.at({{i}}) >= rad_d.at({})) ? (pirb_pos.at({{i}}) - rad_d.at({})) : saw::data<T>{0});
			a.at({i}) = lower.at({{i}}).template cast_to<sch::UInt64>();
			upper.at({{i}}) = pirb_pos.at({{i}}) + rad_d.at({});
			b.at({i}) = (upper.at({{i}})+saw::data<T>{1}).template cast_to<sch::UInt64>();
		}
		
		return aabb;

	}
};
}
}
