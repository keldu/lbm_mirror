#include "../descriptor.h"

#include <forstio/codec/data.h>

namespace schema {
using namespace kel::lbm::schema;

/**
 * Basic distribution function
 * Base type
 * D
 * Q
 * Scalar factor
 * D factor
 * Q factor
 */
using DfCell2DType = CellType<Float32, 2, 5, 0, 0, 1>;

using CellInfo2DType = CellType<UInt8, 2, 5, 1, 0, 0>;

/**
 * Basic type for simulation
 */
using Cell = CellData<
	Member<DfCell2DType, "dfs">,
	Member<CellInfo2DType, "info">
>;

}

int main(){
	using namespace kel::lbm;

	saw::data<schema::Lattice<schema::Cell,2>>, saw::encode::Native> lattice{512, 512};

	return 0;
}
