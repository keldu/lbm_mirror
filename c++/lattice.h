#pragma once

#include <forstio/codec/schema.h>

namespace kel {
namespace lbm {
namespace schema {
using namespace saw::schema;

template<typename T, size_t D>
using Lattice = Array<T,D>;

template<typename T>
using Lattice2D = Lattice<T,2>;

template<typename T>
using Lattice3D = Lattice<T,3>;
}
}
}
