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

	/**
	 * D3Q27 weights, keyed on |e_i|^2:
	 *
	 *   rest   (|e|^2 = 0, 1 of them)   8/27
	 *   face   (|e|^2 = 1, 6 of them)   2/27
	 *   edge   (|e|^2 = 2, 12 of them)  1/54
	 *   corner (|e|^2 = 3, 8 of them)   1/216
	 *
	 * These sum to 1. The six face weights used to read 1/36, which summed to
	 * 13/18 -- equilibrium then returned less mass than it was given and any
	 * 3D run diverged within a handful of steps.
	 */
	static constexpr std::array<typename kel::lbm::native_data_type<T>::type,Q> weights = {
		8./27.,   // 0  rest
		// 1D
		2./27.,   // 1  face
		2./27.,   // 2  face
		// Expand into 2D
		2./27.,   // 3  face
		1./54.,   // 4  edge
		1./54.,   // 5  edge
		2./27.,   // 6  face
		1./54.,   // 7  edge
		1./54.,   // 8  edge
		// Expand into 3D
		2./27.,   // 9  face
		1./54.,   // 10 edge
		1./54.,   // 11 edge
		1./54.,   // 12 edge
		1./216.,  // 13 corner
		1./216.,  // 14 corner
		1./54.,   // 15 edge
		1./216.,  // 16 corner
		1./216.,  // 17 corner
		2./27.,   // 18 face
		1./54.,   // 19 edge
		1./54.,   // 20 edge
		1./54.,   // 21 edge
		1./216.,  // 22 corner
		1./216.,  // 23 corner
		1./54.,   // 24 edge
		1./216.,  // 25 corner
		1./216.   // 26 corner
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,2,1,
		6,8,7,3,5,4,
		18,20,19,24,26,25,21,23,22,9,11,10,15,17,16,12,14,13
	};

	static constexpr typename kel::lbm::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename kel::lbm::native_data_type<T>::type cs2 = 1./3.;
};
}
}
