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

template<typename Func, typename Schema, size_t Dim>
void apply_for_cells(Func&& func, saw::data<schema::Array<Schema, Dim>, saw::encode::Native>& dat ){
	for(std::size_t i = 0; i < dat.get_dim_size(0); ++i){
		for(std::size_t j = 0; j < data.get_dim_size(1); ++j){
			func(data.at(i,j), i, j);
		}
	}
}

int main(){
	using namespace kel::lbm;

	saw::data<schema::Lattice<schema::Cell,2>>, saw::encode::Native> lattice{512, 512};

	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		
	}, lattice);

	return 0;
}
