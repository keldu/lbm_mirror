#include "../descriptor.h"

#include <forstio/codec/data.h>

#include <iostream>

namespace kel {
namespace lbm {
namespace schema {

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
}
}

constexpr size_t dim_x = 32;
constexpr size_t dim_y = 32;

struct rectangle {
	std::array<size_t,4> data_;

	rectangle(size_t x, size_t y, size_t w, size_t h):
		data_{x,y,w,h}
	{}

	bool inside(size_t i, size_t j) const {
		return !(i < data_[0] || i > (data_[0]+data_[2]) || j < data_[1] || j > (data_[1] +data_[3]));
	}
};

template<typename Func, typename Schema, size_t Dim>
void apply_for_cells(Func&& func, saw::data<saw::schema::Array<Schema, Dim>>& dat){
	for(std::size_t i = 0; i < dat.get_dim_size(0); ++i){
		for(std::size_t j = 0; j < dat.get_dim_size(1); ++j){
			func(dat.at(i,j), i, j);
		}
	}
}

void set_geometry(saw::data<kel::lbm::schema::Lattice<kel::lbm::schema::Cell,2>>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		uint8_t val = 0;
		if(i == 1){
			val = 2;
		}
		if(j == 1 || (i+2) == dim_x || (j+2) == dim_y){
			val = 3;
		}
		if(i == 0 || j == 0 || (i+1) == dim_x || (j+1) == dim_y){
			val = 1;
		}
		cell.template get<"info">().at(0).set(val);
	}, latt);
}

void set_initial_conditions(saw::data<kel::lbm::schema::Lattice<kel::lbm::schema::Cell,2>>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		cell.template get<"dfs">().at(0).set(1.0);
	}, latt);
}

int main(){
	using namespace kel::lbm;

	saw::data<schema::Lattice<kel::lbm::schema::Cell,2>, saw::encode::Native> lattice{dim_x, dim_y};

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(lattice);
	/**
	 * 
	 */
	set_initial_conditions(lattice);

	/**
	 * Print basic setup info
	 */
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
			// Not needed
			(void) i;
			std::cout<<static_cast<uint32_t>(cell.template get<"info">().at(0).get());
			if( (j+1) < dim_y){
				std::cout<<" ";
			}else{
				std::cout<<"\n";
			}
	}, lattice);
	
	std::cout<<"\n";
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
			// Not needed
			(void) i;
			std::cout<<cell.template get<"dfs">().at(0).get();
			if( (j+1) < dim_y){
				std::cout<<" ";
			}else{
				std::cout<<"\n";
			}
	}, lattice);

	std::cout<<std::endl;

	return 0;
}
