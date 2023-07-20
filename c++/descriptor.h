#pragma once

#include <forstio/codec/schema.h>

namespace kel {
namespace lbm {
namespace schema {
using namespace saw::schema;

template<typename T, uint64_t D, uint64_t Q>
struct Descriptor {};

template<typename T, uint64_t D, uint64_t Q, uint64_t SC, uint64_t DC, uint64_t QC>
using CellType = FixedArray<T,SC+D*DC+Q*QC>;

/**
 * T... is restricted to Member schemas
 */
template<typename... CellT> 
using CellData = Struct<
	CellT...
>;

/**
 * T is an array of CellData
 */
template<typename T, size_t D>
using Lattice = Array<T, D>;
}
}
}
