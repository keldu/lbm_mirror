#include "../descriptor.h"

#include <forstio/codec/data.hpp>

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
using T = Float32;
using DfCell2D = Field<T, D2Q5, 0, 0, 1>;

using CellInfo2D = Field<UInt8, D2Q5, 1, 0, 0>;

/**
 * Basic type for simulation
 */
using Cell = CellData<
	Member<DfCell2D, "dfs">,
	Member<CellInfo2D, "info">
>;

}

template<typename T, typename Desc, size_t SN, size_t DN, size_t QN>
struct cell_type {
	using Type = schema::Field<T, Desc, SN, DN, QN>;
};

template<typename T>
class df_cell_view;

/**
 * Minor helper for the AA-Pull Pattern
 */
template<typename Desc, size_t SN, size_t DN, size_t QN>
class df_cell_view<cell_type<schema::T, Desc, SN, DN, QN>> {
private:
		std::array<std::decay_t<typename saw::native_data_type<schema::T>::type>*, QN> view_;
public:
		df_cell_view(const std::array<std::decay_t<typename saw::native_data_type<schema::T>::type>*, QN>& view):
				view_{view}
		{}
};

template<typename Desc>
class collision {
public:
	typename saw::native_data_type<schema::T>::type relaxation_;
public:
	std::array<typename saw::native_data_type<schema::T>::type,Desc::Q> equilibrium(
		typename saw::native_data_type<schema::T>::type rho,
		std::array<typename saw::native_data_type<schema::T>::type, Desc::D> vel
	){
		using dfi = df_info<schema::T, Desc>;

		typename std::array<saw::native_data_type<schema::T>::type,Desc::Q> eq;

		for(std::size_t i = 0; i < eq.size(); ++i){
			auto vel_c = (vel[0]*dfi::directions[i][0] + vel[1]*dfi::directions[i][1]);
			auto vel_c_cs2 = vel_c / dfi::cs2;
			eq[i] = dfi::weights[i] * rho * (
				1
				+ vel_c_cs2
				+ vel_c_cs2 * vel_c_cs2
				- ( vel[0] * vel[0] + vel[1] * vel[1] ) / ( 2. * dfi::cs2 )
			);
		}

		return eq;
	}

	void compute_rho_u(
		saw::data<sch::DfCell2D>& dfs,
		typename saw::native_data_type<schema::T>::type& rho,
		std::array<typename saw::native_data_type<schema::T>::type, 2>& vel
	){
		using dfi = df_info<schema::Descriptor<schema::T, D, Q>>;

		rho = 0;
		std::fill(vel.begin(), vel.end(), 0);

		for(size_t i = 0; i < Q; ++i){
			rho += dfs.at(i).get();
			vel[0] += dfi::directions[i][0] * dfs.at(i).get();
			vel[1] += dfi::directions[i][1] * dfs.at(i).get();
		}

		vel[0] /= rho;
		vel[1] /= rho;
	}
};
}
}

constexpr size_t dim_size = 2;
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
		(void) i;
		(void) j;
		cell.template get<"dfs">().at(0).set(1.0);
	}, latt);
}

void lbm_step(
	saw::data<kel::lbm::schema::Lattice<kel::lbm::schema::Cell,2>>& old_latt,
	saw::data<kel::lbm::schema::Lattice<kel::lbm::schema::Cell,2>>& new_latt
){
	
}

int main(){
	using namespace kel::lbm;

	saw::data<
		schema::FixedArray<
			schema::Lattice<kel::lbm::schema::Cell, 2>, 2
		>
		,saw::encode::Native
	> lattices; //{dim_x, dim_y};
	for(uint64_t i = 0; i < lattices.get_dim_size<0u>(); ++i){
		lattices.at(i) = {dim_x, dim_y};
	}

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(lattices.at(0));
	/**
	 * 
	 */
	set_initial_conditions(lattices.at(0));

	/**
	 * Timeloop
	 */

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
	}, lattices.at(0));
	
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
	}, lattices.at(0));

	uint64_t lattice_steps = 32;
	bool even_step = true;

	for(uint64_t step = 0; step < lattice_steps; ++step){
		uint64_t old_lattice_index = even_step ? 0 : 1;
		uint64_t new_lattice_index = even_step ? 1 : 0;

		lbm_step(lattices.at(old_lattice_index), lattices.at(new_lattice_index));

		even_step = !even_step;
	}

	/**
	 * Flush cout
	 */
	std::cout<<"\n\n";
	std::cout.flush();
	return 0;
}
