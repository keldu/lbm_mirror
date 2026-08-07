#pragma once

namespace kel {
namespace lbm {
namespace sch {
using D3Q27 = Descriptor<3u,27u>;
}

template<typename T>
class df_info<T,sch::Descriptor<3, 27>> {
public:
	using Descriptor = sch::Descriptor<3,27>;

	static constexpr uint64_t D = 3u;
	static constexpr uint64_t Q = 27u;

	static constexpr std::array<std::array<int32_t, D>, Q> directions = {{
		{ 0, 0, 0},  // 0
		// Into 1D
		{-1, 0, 0},  // 1
		{ 1, 0, 0},  // 2
		// Expand into 2D
		{ 0, -1, 0}, // 3
		{-1, -1, 0}, // 4
		{ 1, -1, 0}, // 5
		{ 0, 1, 0},  // 6
		{-1, 1, 0},  // 7
		{ 1, 1, 0},  // 8
		// Expand into 3D
		{ 0, 0, -1}, // 9
		{-1, 0, -1}, // 10
		{ 1, 0, -1}, // 11
		{ 0, -1, -1},// 12
		{-1, -1, -1},// 13
		{ 1, -1, -1},// 14
		{ 0, 1, -1}, // 15
		{-1, 1, -1}, // 16
		{ 1, 1, -1}, // 17
		{ 0, 0, 1},  // 18
		{-1, 0, 1},  // 19
		{ 1, 0, 1},  // 20
		{ 0, -1, 1}, // 21
		{-1, -1, 1}, // 22
		{ 1, -1, 1}, // 23
		{ 0, 1, 1},  // 24
		{-1, 1, 1},  // 25
		{ 1, 1, 1}   // 26
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
		8./27.,
		// 1D
		1./36.,
		1./36.,
		// 2D
		1./36.,
		1./54.,
		1./54.,
		1./36.,
		1./54.,
		1./54.,
		// 3D
		1./36.,
		1./54.,
		1./54.,
		1./54.,
		1./216.,
		1./216.,
		1./54.,
		1./216.,
		1./216.,
		1./36.,
		1./54.,
		1./54.,
		1./54.,
		1./216.,
		1./216.,
		1./54.,
		1./216.,
		1./216.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,2,1,
		6,8,7,3,5,4,
		18,20,19,24,26,25,21,23,22,9,11,10,15,17,16,12,14,13
	};

	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
}
}
