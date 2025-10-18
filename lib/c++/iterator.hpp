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
