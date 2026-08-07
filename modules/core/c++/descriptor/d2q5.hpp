#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace sch {
using D2Q9 = Descriptor<2u,5u>;
}

template<typename T>
class df_info<T,sch::Descriptor<2, 5>> {
public:
	using Descriptor = sch::Descriptor<2,5>;
	
	static constexpr uint64_t D = 2u;
	static constexpr uint64_t Q = 5u;

	static constexpr std::array<std::array<int32_t, D>, Q> directions = {{
		{ 0, 0}, // 0
		{-1, 0}, // 1
		{ 1, 0}, // 2
		{ 0,-1}, // 3
		{ 0, 1}  // 4
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
		1./3.,
		1./6.,
		1./6.,
		1./6.,
		1./6.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,
		2,
		1,
		4,
		3
	};
	
	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};


}
}
