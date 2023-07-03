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
using DfCellType = CellType<Float32, 2, 5, 0, 0, 1>;

using CellInfoType = CellType<UInt8, 2, 5, 1, 0, 0>;

using CellData = CellData<
	Member<DfCellType, "dfs">,
	Member<CellInfoType, "info">
>;
}

int main(){
	using namespace kel::lbm;

	saw::data<schema::CellData, saw::encode::Native> lattice{512, 512};

	return 0;
}
