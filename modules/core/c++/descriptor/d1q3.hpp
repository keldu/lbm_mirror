#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace sch {
using D1Q3 = Descriptor<1u,3u>;
}

template<typename T> 
class df_info<T,sch::Descriptor<1,3>> {
public:
	using Descriptor = sch::Descriptor<1,3>;

	static constexpr uint64_t D = 1u;
	static constexpr uint64_t Q = 3u;

	static constexpr std::array<std::array<int32_t,D>,Q> directions = {{
		{ 0},
		{-1},
		{ 1}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type, Q> weights = {
		2./3.,
		1./6.,
		1./6.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,2,1
	};
	
	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};

}
}
