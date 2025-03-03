#pragma once

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<uint64_t D, uint64_t Q>
struct Descriptor {
	static constexpr uint64_t D = D;
	static constexpr uint64_t Q = Q;
};

template<typename Sch, typename Desc, uint64_t SC, uint64_t DC, uint64_t QC>
struct Field {
	using Alias = Sch;
	using Descriptor = Desc;
	static constexpr uint64_t SC = SC;
	static constexpr uint64_t DC = DC;
	static constexpr uint64_t QC = QC;
};

/**
 * T... is restricted to Member schemas
 */
template<typename... CellT> 
using CellData = Struct<
	CellT...
>;
^
/**
 * T is an array of CellData
 */
template<typename T, size_t D>
using Lattice = Array<T, D>;
}

template<typename T, typename Desc>
class df_info{};

template<typename T>
class df_info<T,sch::Descriptor<2, 5>> {
	static constexpr std::array<std::array<int32_t, 2>, 5> directions = {{
		{ 0, 0},
		{-1, 0},
		{ 0,-1},
		{ 0, 1},
		{ 1, 0}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,5> weights = {
		1./3.,
		1./6.,
		1./6.,
		1./6.,
		1./6.
	};

	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
}
}
