#include "../descriptor.h"

#include <forstio/codec/data.hpp>

#include <iostream>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

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
using D2Q5 = Descriptor<2,5>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0, 0, 1>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q5, 1, 0, 0>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<CellInfo<Desc>, "info">
>;


using CavityFieldD2Q5 = Field<D2Q5, CellStruct<D2Q5>>;
}

/*
template<typename T, typename Encode>
class df_cell_view;
*/
/**
 * Minor helper for the AA-Pull Pattern
 */
/*
template<typename Desc, size_t SN, size_t DN, size_t QN, typename Encode>
class df_cell_view<sch::Cell<sch::T, Desc, SN, DN, QN>, Encode> {
public:
	using Schema = sch::Cell<sch::T,Desc,SN,DN,QN>;
private:
		std::array<std::decay_t<typename saw::native_data_type<sch::T>::type>*, QN> view_;
public:
		df_cell_view(const std::array<std::decay_t<typename saw::native_data_type<sch::T>::type>*, QN>& view):
				view_{view}
		{}
};
*/

template<typename Desc>
class bounce_back {
public:

	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		using dfi = df_info<sch::T,Desc>;

		// Technically use .copy()
		auto df_cpy = dfs;

		for(uint64_t i = 1u; i < Desc::Q; ++i){
			dfs({i}) = df_cpy({dfi::opposite_index.at(i)});
		}
	}
};

template<typename Desc>
class cavity_boundary {
public:
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> lid_vel;
	
public:
	void compute_rho_u(
		saw::data<sch::DfCell<Desc>>& dfs,
		typename saw::native_data_type<sch::T>::type& rho,
		std::array<typename saw::native_data_type<sch::T>::type, 2>& vel
	){
		using dfi = df_info<sch::T, Desc>;

		rho = 0;
		std::fill(vel.begin(), vel.end(), 0);

		for(size_t i = 0; i < Desc::Q; ++i){
			rho += dfs(i).get();
			vel[0] += dfi::directions[i][0] * dfs(i).get();
			vel[1] += dfi::directions[i][1] * dfs(i).get();
		}

		vel[0] /= rho;
		vel[1] /= rho;
	}

	void apply(
		saw::data<sch::DfCell<Desc>>& dfs
	){
		using dfi = df_info<sch::T,Desc>;
		
		// Technically use .copy()
		auto df_cpy = dfs;
		
		typename saw::native_data_type<sch::T>::type rho;
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
		compute_rho_u(dfs,rho,vel);

		for(uint64_t i = 1u; i < Desc::Q; ++i){
			dfs({i}) = df_cpy({dfi::opposite_index.at(i)}) - dfi::weights[i] * rho * ( lid_vel[0] * dfi::directions[i][0] + lid_vel[1] * dfi::directions[i][1]) / dfi::cs2;
		}
	}
};

template<typename Desc>
class collision {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
public:

	std::array<typename saw::native_data_type<sch::T>::type,Desc::Q> equilibrium(
		typename saw::native_data_type<sch::T>::type rho,
		const std::array<typename saw::native_data_type<sch::T>::type, Desc::D>& vel
	){
		using dfi = df_info<sch::T, Desc>;

		typename std::array<saw::native_data_type<sch::T>::type,Desc::Q> eq;

		for(std::size_t i = 0; i < eq.size(); ++i){
			auto vel_c = (vel[0]*dfi::directions[i][0] + vel[1]*dfi::directions[i][1]);
			auto vel_c_cs2 = vel_c * dfi::inv_cs2;
			eq[i] = dfi::weights[i] * rho * (
				1
				+ vel_c_cs2
				+ vel_c_cs2 * vel_c_cs2 * 0.5
				- dfi::inv_cs2 * 0.5 * ( vel[0] * vel[0] + vel[1] * vel[1] )
			);
		}

		return eq;
	}

	void compute_rho_u(
		saw::data<sch::DfCell<Desc>>& dfs,
		typename saw::native_data_type<sch::T>::type& rho,
		std::array<typename saw::native_data_type<sch::T>::type, 2>& vel
	){
		using dfi = df_info<sch::T, Desc>;

		rho = 0;
		std::fill(vel.begin(), vel.end(), 0);

		for(size_t i = 0; i < Desc::Q; ++i){
			rho += dfs(i).get();
			vel[0] += dfi::directions[i][0] * dfs(i).get();
			vel[1] += dfi::directions[i][1] * dfs(i).get();
		}

		vel[0] /= rho;
		vel[1] /= rho;
	}

	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		for(uint64_t i = 0u; i < Desc::Q; ++i){
			typename saw::native_data_type<sch::T>::type rho;
			std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
			compute_rho_u(dfs,rho,vel);
			auto eq = equilibrium(rho,vel);

			dfs({i}).set(dfs({i}).get() + (1.0 / relaxation_) * (eq[i] - dfs({i}).get()));
		}
	}
};
}
}

constexpr size_t dim_size = 2;
constexpr size_t dim_x = 64;
constexpr size_t dim_y = 64;

struct rectangle {
	std::array<size_t,4> data_;

	rectangle(size_t x, size_t y, size_t w, size_t h):
		data_{x,y,w,h}
	{}

	bool inside(size_t i, size_t j) const {
		return !(i < data_[0] || i > (data_[0]+data_[2]) || j < data_[1] || j > (data_[1] +data_[3]));
	}
};

template<typename Func>
void apply_for_cells(Func&& func, saw::data<kel::lbm::sch::CavityFieldD2Q5>& dat){
	for(std::size_t i = 0; i < dat.template get_dim_size<0>().get(); ++i){
		for(std::size_t j = 0; j < dat.template get_dim_size<1>().get(); ++j){
			saw::data<saw::schema::UInt64> di{i};
			saw::data<saw::schema::UInt64> dj{j};
			auto& cell_v = dat({{di,dj}});
			func(cell_v, i, j);
		}
	}
}

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q5>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		uint8_t val = 0;
		if(i == 1){
			val = 2u;
		}
		if(j == 1 || (i+2) == dim_x || (j+2) == dim_y){
			val = 3u;
		}
		if(i == 0 || j == 0 || (i+1) == dim_x || (j+1) == dim_y){
			val = 1u;
		}
		cell.template get<"info">()(0u).set(val);
	}, latt);
}

void set_initial_conditions(saw::data<kel::lbm::sch::CavityFieldD2Q5>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		(void) i;
		(void) j;
		auto& dfs = cell.template get<"dfs">();
		dfs(0).set(1.0);
	}, latt);
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q5>& old_latt,
	saw::data<kel::lbm::sch::CavityFieldD2Q5>& new_latt
){
	using namespace kel::lbm;
  using dfi = df_info<sch::T,sch::D2Q5>;

	collision<sch::D2Q5> coll;
	coll.relaxation_ = 1.0;

	bounce_back<sch::D2Q5> bb;
	cavity_boundary<sch::D2Q5> bb_lid;
	bb_lid.lid_vel = {0.1,0.0};

	apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
		auto& df = cell.template get<"dfs">();
		auto& info = cell.template get<"info">();

		auto info_val = info({0u}).get();
		switch(info_val){
		case 0u:
			coll.apply(df);
			break;
		case 2u:
			bb_lid.apply(df);
			break;
		case 3u:
			bb.apply(df);
			break;
		}
	}, old_latt);

	for(uint64_t i = 1; (i+1) < old_latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1; (j+1) < old_latt.template get_dim_size<1>().get(); ++j){
			auto& df_new = new_latt({{i,j}}).template get<"dfs">();

			for(uint64_t k = 0u; k < sch::D2Q5::Q; ++k){
				auto dir = dfi::directions[dfi::opposite_index[k]];
				
				auto& df_old = old_latt({{i+dir[0],j+dir[1]}}).template get<"dfs">();
				df_new({k}) = df_old({k});
			}
		}
	}
}

int main(){
	using namespace kel::lbm;

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q5::D>> dim{{dim_x, dim_y}};

	saw::data<sch::CavityFieldD2Q5, saw::encode::Native> old_lattice{dim};
	saw::data<sch::CavityFieldD2Q5, saw::encode::Native> new_lattice{dim};

	// auto& df_field = lattices.at(0).template get<"dfs">();
	//for(uint64_t i = 0; i < df_field.get_dim_size<0u>(); ++i){
	//	lattices.at(i) = {dim_x, dim_y};
	//}

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(old_lattice);
	set_geometry(new_lattice);
	/**
	 * 
	 */
	set_initial_conditions(old_lattice);
	set_initial_conditions(new_lattice);

	/**
	 * Timeloop
	 */

	/**
	 * Print basic setup info
	 */
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
			// Not needed
			(void) i;
			std::cout<<(static_cast<uint32_t>(cell.template get<"info">()({0}).get()));
			if( (j+1) < dim_y){
				std::cout<<" ";
			}else{
				std::cout<<"\n";
			}
	}, old_lattice);
	
	std::cout<<"\n";
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
			// Not needed
			(void) i;
			std::cout<<cell.template get<"dfs">()({0}).get();
			if( (j+1) < dim_y){
				std::cout<<" ";
			}else{
				std::cout<<"\n";
			}
	}, old_lattice);

	uint64_t lattice_steps = 16u;
	bool even_step = true;

	for(uint64_t step = 0; step < lattice_steps; ++step){
		auto& old_lat = even_step ? old_lattice : new_lattice;
		auto& new_lat = even_step ? new_lattice : old_lattice;


		std::cout<<"\n";
		apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
				// Not needed
				(void) i;
				std::cout<<cell.template get<"dfs">()({0}).get();
				if( (j+1) < dim_y){
					std::cout<<" ";
				}else{
					std::cout<<"\n";
				}
		}, old_lat);
		
		lbm_step(old_lat, new_lat);

		even_step = !even_step;
	}

	/**
	 * Flush cout
	 */
	std::cout<<"\n\n";
	std::cout.flush();
	return 0;
}
