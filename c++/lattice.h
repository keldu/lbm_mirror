#pragma once

#include <forstio/codec/schema.h>

namespace kel {
namespace lbm {
namespace schema {
using namespace saw::schema;

template<typename T, uint16_t D, uint16_t Q>
using Lattice = Array<FixedArray<T,Q>,D>;

template<typename T, uint16_t Q>
using Lattice2D = Lattice<T,2,Q>;

template<typename T, uint16_t Q>
using Lattice3D = Lattice<T,3,Q>;
}
}
}
