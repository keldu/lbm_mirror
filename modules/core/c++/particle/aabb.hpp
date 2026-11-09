#pragma once

#include "common.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {
template<typename PGroup>
class particle_aabb final {
	static_assert(kel::lbm::always_false<PGroup>, "Not supported");
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
	template<typename Sch, typename Encode>
	static constexpr kel::lbm::error_or<kel::lbm::data<AABB>> calculate(const kel::lbm::data<Sch,Encode>& p_grp, const kel::lbm::data<sch::FixedArray<sch::UInt64,1u>>& index, const kel::lbm::data<sch::FixedArray<sch::UInt64,D>>& meta){
		static_assert(PC > 0u, "Can't calculate from no particles");
		if(not (index.at({{0u}}).get() < PC) ){
			return kel::lbm::make_error<kel::lbm::err::critical>("Too large i in particle_aabb");
		}

		kel::lbm::data<AABB> aabb;
		auto& parts = p_grp.template get<"particles">();

		auto& pi = parts.at(index);
		auto& pirb = pi.template get<"rigid_body">();
		auto& pirb_pos = pirb.template get<"position">();

		auto& a = aabb.template get<"a">();
		auto& b = aabb.template get<"b">();

		// Radius is per particle now, so a group can hold mixed sizes.
		const kel::lbm::data<sch::Scalar<T>>& rad_d = pi.template get<"collision">().template get<"radius">();

		kel::lbm::data<sch::Vector<T,D>> lower;
		kel::lbm::data<sch::Vector<T,D>> upper;

		for(uint64_t i{0u}; i < D; ++i){
			lower.at({{i}}) = ((pirb_pos.at({{i}}) >= rad_d.at({})) ? (pirb_pos.at({{i}}) - rad_d.at({})) : kel::lbm::data<T>{0});
			a.at({i}) = lower.at({{i}}).template cast_to<sch::UInt64>();

			upper.at({{i}}) = pirb_pos.at({{i}}) + rad_d.at({});
			/**
			 * Clamp against the field extent. Callers hand the result straight
			 * to iterator<D>::apply as [a,b), so an unclamped b reads past the
			 * end for any particle within a radius of the far wall.
			 *
			 * Note this bounds the cell itself, not its neighbours: HLBM also
			 * reads neighbour_at on dfs, which is safe because DfChunk carries
			 * Ghost = 1. The macro chunks have Ghost = 0 and are only ever
			 * read at the cell itself.
			 */
			auto b_i = (upper.at({{i}})+kel::lbm::data<T>{1}).template cast_to<sch::UInt64>();
			b.at({i}) = (b_i < meta.at({i})) ? b_i : meta.at({i});
		}
		
		return aabb;
	}
};

}
}
