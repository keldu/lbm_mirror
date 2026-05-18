#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {
template<typename Func>
void iterate_over(Func&& func, const saw::data<sch::FixedArray<sch::UInt64,2u>>& start, const saw::data<sch::FixedArray<sch::UInt64,2u>>& end, const saw::data<sch::FixedArray<sch::UInt64,2u>>& dist = {{{0u,0u}}}){
	// static_assert(D == 2u, "Currently a lazy implementation for AND combinations of intervalls.");
	for(saw::data<sch::UInt64> i{start.at({0u}) + dist.at({0u})}; (i+dist.at({0u})) < end.at({0u}); ++i){
		for(saw::data<sch::UInt64> j{start.at({1u}) + dist.at({1u})}; (j+dist.at({1u})) < end.at({1u}); ++j){
			func({{i,j}});
		}
	}
	return;
}

/**
 *
 */
template<uint64_t D>
struct iterator {
private:
	template<uint64_t i, typename Func>
	static void iterate_over_i(Func& func,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& start,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& end,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& dist,
		saw::data<sch::FixedArray<sch::UInt64,D>>& iter
	){
		static_assert(i <= D, "Eh. Too tired to think of a good message");
		if constexpr ( i == D ){
			func(iter);
		}else{
			for(iter.at({i}) = start.at({i})+dist.at({i}); (iter.at({i})+dist.at({i}) < end.at({i})); ++iter.at({i})){
				iterate_over_i<i+1u,Func>(func,start,end,dist,iter);
			}
		}
	}
public:
	template<typename Func>
	static void apply(
		Func&& func,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& start,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& end,
		const saw::data<sch::FixedArray<sch::UInt64,D>>& dist = {}
	){
		saw::data<sch::FixedArray<sch::UInt64,D>> iter;
		iterate_over_i<0u,Func>(func, start, end, dist, iter);
	}
};

template<typename... Tup>
struct ct_tuple_iterator final {
private:
	template<typename Func, uint64_t i>
	static constexpr saw::error_or<void> apply_i(
		Func& func,
		saw::data<sch::Tuple<Tup...>>& tup
	){
		if constexpr ( i < sizeof...(Tup) ){
			auto eov = func(tup.template get<i>());
			if(eov.is_error()){
				return eov;
			}
			return apply_i<Func,i+1u>(func,tup);
		}

		return saw::make_void();
	}
public:
	template<typename Func>
	static constexpr saw::error_or<void> apply(
		Func&& func,
		saw::data<sch::Tuple<Tup...>>& tup
	){
		return apply_i<Func,0u>(func,tup);
	}
};

/* Ambiguous
template<typename Func>
void iterate_over(Func&& func, const saw::data<sch::FixedArray<sch::UInt64,3u>>& start, const saw::data<sch::FixedArray<sch::UInt64,3u>>& end, const saw::data<sch::FixedArray<sch::UInt64,3u>>& dist = {{{0u,0u,0u}}}){
	// static_assert(D == 2u, "Currently a lazy implementation for AND combinations of intervalls.");
	for(saw::data<sch::UInt64> i{start.at({0u}) + dist.at({0u})}; (i+dist.at({0u})) < end.at({0u}); ++i){
		for(saw::data<sch::UInt64> j{start.at({1u}) + dist.at({1u})}; (j+dist.at({1u})) < end.at({1u}); ++j){
			for(saw::data<sch::UInt64> k{start.at({2u}) + dist.at({2u})}; (j+dist.at({2u})) < end.at({2u}); ++j){
				func({{k,j,i}});
			}
		}
	}
	return;
}
*/
}
}
