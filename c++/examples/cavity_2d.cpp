#include "../descriptor.h"

/**
 */
#include <forstio/codec/data.hpp>

#include <iostream>
#include <cmath>

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
using D2Q9 = Descriptor<2,9>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0, 0, 1>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q9, 1, 0, 0>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<CellInfo<Desc>, "info">
>;


using CavityFieldD2Q9 = Field<D2Q9, CellStruct<D2Q9>>;
}

/**
 * Calculate the macroscopic variables rho and u in Lattice Units.
 */
template<typename Desc>
void compute_rho_u (
		saw::data<sch::DfCell<Desc>>& dfs,
		typename saw::native_data_type<sch::T>::type& rho,
		std::array<typename saw::native_data_type<sch::T>::type, 2>& vel
	)
{
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

/**
 * Calculates the equilibrium for each direction
 */
template<typename Desc>
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

/**
 * A reason for why a component based design is really good can be seen in my LR solver example
 *
 * Add Expression Templates and you're golden.
 */
template<typename Kind, typename Desc>
class component;

/*
template<typename T, typename Encode>
class df_cell_view;
*/
/**
 * Minor helper for the AA-Pull Pattern, so I can use only one lattice
 *
 * Am I sure I want to use AA this way?
 * Esoteric Twist technically reduces the needed memory access footprint
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
namespace cmpt {
struct BounceBack{};
template<typename ColliderComponent>
struct MovingWall {};
struct BGK {};
}

/**
 * Full-Way BounceBack. I suspect that the moving wall requires half-way bounce back.
 */
template<typename Desc>
class component<cmpt::BounceBack,Desc> {
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

/**
 * Full-Way moving wall Bounce back, something is not right here.
 * Technically it should reflect properly.
 */
template<typename CollisionCmp, typename Desc>
class component<cmpt::MovingWall<CollisionCmp>, Desc> {
public:
	component<CollisionCmp, Desc>* collision_;
	std::array<typename saw::native_data_type<sch::T>::type, Desc::D> lid_vel;
	
public:
	void apply(
		saw::data<sch::DfCell<Desc>>& dfs
	){
		using dfi = df_info<sch::T,Desc>;

		collision_->apply(dfs);
		
		// Technically use .copy()
	}
};

template<typename Desc>
class component<cmpt::BGK, Desc> {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
public:
	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		for(uint64_t i = 0u; i < Desc::Q; ++i){
			typename saw::native_data_type<sch::T>::type rho;
			std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
			compute_rho_u<Desc>(dfs,rho,vel);
			auto eq = equilibrium<Desc>(rho,vel);

			dfs({i}).set(dfs({i}).get() + (1.0 / relaxation_) * (eq[i] - dfs({i}).get()));
		}
	}
};
}
}

constexpr size_t dim_size = 2;
constexpr size_t dim_x = 128;
constexpr size_t dim_y = 128;


template<typename Func>
void apply_for_cells(Func&& func, saw::data<kel::lbm::sch::CavityFieldD2Q9>& dat){
	for(std::size_t i = 0; i < dat.template get_dim_size<0>().get(); ++i){
		for(std::size_t j = 0; j < dat.template get_dim_size<1>().get(); ++j){
			saw::data<saw::schema::UInt64> di{i};
			saw::data<saw::schema::UInt64> dj{j};
			auto& cell_v = dat({{di,dj}});
			func(cell_v, i, j);
		}
	}
}

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		uint8_t val = 0;
		if(i == 1){
			if( j > 2 && (j+3) < dim_y ){
				val = 2u;
			}else{
				val = 3u;
			}
		}
		if(j == 1 || (i+2) == dim_x || (j+2) == dim_y){
			val = 3u;
		}
		if(i == 0 || j == 0 || (i+1) == dim_x || (j+1) == dim_y){
			val = 0u;
		}
		if(i > 1 && (i+2) < dim_x && j > 1 && (j+2) < dim_y){
			val = 1u;
		}
		cell.template get<"info">()(0u).set(val);
	}, latt);
}

void set_initial_conditions(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;
	
	typename saw::native_data_type<sch::T>::type rho = 1.0;
	{
		std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel = {0.0, 0.0};
		auto eq = equilibrium<sch::D2Q9>(rho, vel);

		apply_for_cells([&eq](auto& cell, std::size_t i, std::size_t j){
			(void) i;
			(void) j;
			auto& dfs = cell.template get<"dfs">();
			for(uint64_t i = 0; i < sch::D2Q9::Q; ++i){
				dfs(i).set(eq[i]);
			}
		}, latt);
	}
	
	{
		std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel = {0.1, 0.0};
		auto eq = equilibrium<sch::D2Q9>(rho, vel);

		apply_for_cells([&eq](auto& cell, std::size_t i, std::size_t j){
			(void) i;
			(void) j;
			auto& dfs = cell.template get<"dfs">();
			auto info = cell.template get<"info">()(0u).get();
			if(info == 2u){
				for(uint64_t i = 0; i < sch::D2Q9::Q; ++i){
					dfs(i).set(eq[i]);
				}
			}
		}, latt);
	}

}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& old_latt,
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& new_latt
){
	using namespace kel::lbm;
  using dfi = df_info<sch::T,sch::D2Q9>;

	component<cmpt::BGK,sch::D2Q9> coll;
	coll.relaxation_ = 0.5384;

	component<cmpt::BounceBack,sch::D2Q9> bb;
	component<cmpt::MovingWall<cmpt::BGK>,sch::D2Q9> bb_lid;
	bb_lid.collision_ = &coll;
	bb_lid.lid_vel = {0.1,0.0};

	// Collide
	apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
		auto& df = cell.template get<"dfs">();
		auto& info = cell.template get<"info">();

		auto info_val = info({0u}).get();
		switch(info_val){
		case 1u:
			coll.apply(df);
			break;
		case 2u:
			// bb.apply(df);
			bb_lid.apply(df);
			break;
		case 3u:
			bb.apply(df);
			break;
		}
	}, old_latt);

	// Stream
	for(uint64_t i = 1; (i+1) < old_latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1; (j+1) < old_latt.template get_dim_size<1>().get(); ++j){
			auto& cell_new = new_latt({{i,j}});
			auto& df_new = cell_new.template get<"dfs">();
			auto& info_new = cell_new.template get<"info">();
			auto df_cpy_old = old_latt({{i,j}}).template get<"dfs">();

			for(uint64_t k = 0u; k < sch::D2Q9::Q; ++k){
				auto dir = dfi::directions[dfi::opposite_index[k]];
				
				auto& cell_old = old_latt({{i+dir[0],j+dir[1]}});
				auto& df_old = cell_old.template get<"dfs">();
				auto& info_old = cell_old.template get<"info">();

				if(info_new({0}).get() == 2u && info_old({0}).get() == 0u){
						// Density set to 1.0
						df_new({dfi::opposite_index.at(i)}) = df_cpy_old({i}) - 2.0 * dfi::weights[i] * 1.0 * ( bb_lid.lid_vel[0] * dfi::directions[i][0] + bb_lid.lid_vel[1] * dfi::directions[i][1]) * dfi::inv_cs2;
				}else{
					df_new({k}) = df_old({k});
				}
			}
		}
	}
}

int main(){
	using namespace kel::lbm;

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{dim_x, dim_y}};

	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> old_lattice{dim};
	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> new_lattice{dim};

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
	
	uint64_t lattice_steps = 37000u;
	bool even_step = true;

	uint64_t print_every = 128u;

	for(uint64_t step = 0; step < lattice_steps; ++step){
		auto& old_lat = even_step ? old_lattice : new_lattice;
		auto& new_lat = even_step ? new_lattice : old_lattice;


		std::cout<<"\n";

		if(step % print_every == 0u){
			typename saw::native_data_type<sch::T>::type sum = 0.0;

			apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
				auto& dfs = cell.template get<"dfs">();
				typename saw::native_data_type<sch::T>::type rho;
				std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel;
				compute_rho_u<sch::D2Q9>(dfs,rho,vel);

				if(i > 1 && (i+2) < dim_x && j > 1 && (j+2) < dim_y){
					sum += rho;
				}
				auto angle = std::atan2(vel[1],vel[0]);
				
				auto dir_char = [&]() -> std::string_view {
					constexpr auto pi = M_PI;
					if((vel[1] * vel[1] + vel[0]*vel[0]) < 1e-16){
						return "•";
					}
					if(angle > 7.0 / 8.0 * pi){
						return "←";
					}
					if(angle > 5.0 / 8.0 * pi){
						return "↖";
					}
					if(angle > 3.0 / 8.0 * pi){
						return "↑";
					}
					if(angle > 1.0 / 8.0 * pi){
						return "↗";
					}
					if(angle > -1.0 / 8.0 * pi){
						return "→";
					}
					if(angle > -3.0 / 8.0 * pi){
						return "↘";
					}
					if(angle > -5.0 / 8.0 * pi){
						return "↓";
					}
					return "←";
				}();
				std::cout<<dir_char;
				if( (j+1) < dim_y){
					std::cout<<" ";
				}else{
					std::cout<<"\n";
				}
			}, old_lat);

			std::cout<<"Average rho: "<<(sum / ((dim_x-4)*(dim_y-4)))<<std::endl;
		}
		
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
