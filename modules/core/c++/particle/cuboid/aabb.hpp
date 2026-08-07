#pragma once

#include "common.hpp"
#include "../aabb.hpp"

namespace kel {
namespace lbm {

template<typename T, uint64_t D, uint64_t PC>
class particle_aabb<
	sch::ParticleGroup<T,D,PC,coll::Cuboid<T>>
> final {
public:
	using Schema = sch::ParticleGroup<T,D,PC,coll::Cuboid<T>>;

	using AABB = sch::Struct<
		sch::Member<sch::FixedArray<sch::UInt64,D>, "a">,
		sch::Member<sch::FixedArray<sch::UInt64,D>, "b">
	>;

public:
	template<typename Sch, typename Encode>
	static constexpr saw::error_or<saw::data<AABB>> calculate(const saw::data<Sch,Encode>& pg, const saw::data<sch::FixedArray<sch::UInt64,1u>>& index, const saw::data<sch::FixedArray<sch::UInt64,D>>& meta){
		static_assert(PC > 0u, "Can't calculate from no particle");
		if(not (index.at({0u}).get() < PC)){
			return saw::make_error<saw::err::critical>("Too large i in particle_aabb coll::Cuboid<T>");
		}

		saw::data<AABB> aabb;
		auto& parts = pg.template get<"particles">();
		auto& pi = parts.at(index);
		auto& pirb = pi.template get<"rigid_body">();

		/// TODO

		return aabb;
	}
};
}
}
