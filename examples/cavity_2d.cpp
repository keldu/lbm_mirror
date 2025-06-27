#include "../c++/descriptor.hpp"
#include "../c++/macroscopic.hpp"
#include "../c++/lbm.hpp"
#include "../c++/component.hpp"
#include "../c++/collision.hpp"
#include "../c++/boundary.hpp"

#include <forstio/codec/data.hpp>

#include <iostream>
#include <fstream>
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
using D2Q5 = Descriptor<2u,5u>;
using D2Q9 = Descriptor<2u,9u>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0u, 0u, 1u>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q9, 1u, 0u, 0u>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">
>;

using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;


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
struct MovingWall {};
}

/**
 * Full-Way moving wall Bounce back, something is not right here.
 * Technically it should reflect properly.
 */
template<typename Desc>
class component<sch::T, Desc, cmpt::MovingWall> {
public:
	std::array<typename saw::native_data_type<sch::T>::type, Desc::D> lid_vel;
	
public:
	void apply(
		saw::data<sch::DfCell<Desc>>& dfs
	){
		using dfi = df_info<sch::T,Desc>;

		// Technically use .copy()
		/*
		auto dfs_cpy = dfs;
		
		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs({dfi::opposite_index.at(i)}) = dfs_cpy({i}) - 2.0 * dfi::weights[i] * 1.0 * ( lid_vel[0] * dfi::directions[i][0] + lid_vel[1] * dfi::directions[i][1]) * dfi::inv_cs2;
		}
		*/
	}
};
}
}

constexpr size_t dim_size = 2;
constexpr size_t dim_x = 128;
constexpr size_t dim_y = 128;


template<typename Func>
void apply_for_cells(Func&& func, saw::data<kel::lbm::sch::CavityFieldD2Q9>& dat){
	for(std::size_t i = 0; i < dat.meta().at({1u}).get(); ++i){
		for(std::size_t j = 0; j < dat.meta().at({0u}).get(); ++j){
			saw::data<saw::schema::UInt64> di{i};
			saw::data<saw::schema::UInt64> dj{j};
			auto& cell_v = dat({{dj,di}});
			func(cell_v, j, i);
		}
	}
}

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		uint8_t val = 0;
		if(j == 1){
			val = 2u;
		}
		if(i == 1 || (i+2) == dim_x || (j+2) == dim_y){
			val = 3u;
		}
		if(i > 1 && (i+2) < dim_x && j > 1 && (j+2) < dim_y){
			val = 1u;
		}
		if(i == 0 || j == 0 || (i+1) == dim_x || (j+1) == dim_y){
			val = 0u;
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
			auto& dfs_old = cell.template get<"dfs_old">();
			auto info = cell.template get<"info">()(0u).get();
			for(uint64_t k = 0; k < sch::D2Q9::Q; ++k){
				dfs(k).set(eq[k]);
				dfs_old(k).set(eq[k]);
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
			auto& dfs_old = cell.template get<"dfs_old">();
			auto info = cell.template get<"info">()(0u).get();
			if(info == 2u){
				for(uint64_t k = 0; k < sch::D2Q9::Q; ++k){
					dfs(k).set(eq[k]);
					dfs_old(k).set(eq[k]);
				}
			}
		}, latt);
	}
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
	bool even_step,
	uint64_t time_step
){
	using namespace kel::lbm;
  using dfi = df_info<sch::T,sch::D2Q9>;

  /**
	* 1. Relaxation parameter \tau
	*/
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.5384};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;

	component<sch::T, sch::D2Q9, cmpt::MovingWall> bb_lid;
	bb_lid.lid_vel = {0.1,0.0};

	auto meta = latt.meta();

	// Collide
	for(saw::data<sch::UInt64> i{0u}; i < meta.at(0u); ++i){
		for(saw::data<sch::UInt64> j{0u}; j < meta.at(1u); ++j ){
			auto& cell = latt({{i,j}});
			auto& info = cell.template get<"info">();

			switch(info({0u}).get()){
				case 1u: {
					coll.apply(latt, {{i,j}}, time_step);
					break;
				}
				case 2u: {
					auto& df = even_step ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
					bb_lid.apply(df);
					break;
				}
				case 3u: {
					bb.apply(latt, {{i,j}}, time_step);
					break;
				}
				default:
					break;
			}
		}
	}

	// Stream
	for(uint64_t i = 1u; (i+1u) < latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1u; (j+1u) < latt.template get_dim_size<1>().get(); ++j){
			auto& cell = latt({{i,j}});
			auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
			auto& info_new = cell.template get<"info">();

			if(info_new({0u}).get() > 0u && info_new({0u}).get() != 2u){
				for(uint64_t k = 0u; k < sch::D2Q9::Q; ++k){
					auto dir = dfi::directions[dfi::opposite_index[k]];
					auto& cell_dir_old = latt({{i+dir[0],j+dir[1]}});
					
					auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();
					auto& info_old = cell_dir_old.template get<"info">();

					if( info_old({0}).get() == 2u ){
						auto& df_old_loc = even_step ? latt({{i,j}}).template get<"dfs_old">() : latt({{i,j}}).template get<"dfs">();
						df_new({k}) = df_old_loc({dfi::opposite_index.at(k)}) - 2.0 * dfi::inv_cs2 * dfi::weights.at(k) * 1.0 * ( bb_lid.lid_vel[0] * dir[0] + bb_lid.lid_vel[1] * dir[1]);
						// dfs({dfi::opposite_index.at(i)}) = dfs_cpy({i}) - 2.0 * dfi::weights[i] * 1.0 * ( lid_vel[0] * dfi::directions[i][0] + lid_vel[1] * dfi::directions[i][1]) * dfi::inv_cs2;
					} else {
						df_new({k}) = df_old({k});
					}
				}
			}
		}
	}
}

int main(){
	using namespace kel::lbm;

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{dim_x, dim_y}};

	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> lattice{dim};

	converter<sch::T> conv{
		{0.1},
		{0.1}
	};

	print_lbm_meta<sch::T, sch::D2Q9>(conv, {1e-3});

	//auto& df_field = lattices.at(0).template get<"dfs">();
	//for(uint64_t i = 0; i < df_field.get_dim_size<0u>(); ++i){
	//	lattices.at(i) = {dim_x, dim_y};
	//}

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(lattice);
	
	/**
	 * 
	 */
	set_initial_conditions(lattice);

	/**
	 * Timeloop
	 */

	/**
	 * Print basic setup info
	 */
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
			// Not needed
			(void) j;
			std::cout<<(static_cast<uint32_t>(cell.template get<"info">()({0}).get()));
			if( (i+1) < dim_x){
				std::cout<<" ";
			}else{
				std::cout<<"\n";
			}
	}, lattice);
	
	uint64_t lattice_steps = 512000u;
	bool even_step = true;

	uint64_t print_every = 256u;
	uint64_t file_no = 0u;

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{dim};

	for(uint64_t step = 0; step < lattice_steps; ++step){

		if(step % print_every == 0u){
			std::cout<<"\n";
			typename saw::native_data_type<sch::T>::type sum = 0.0;


			apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
				auto& dfs = cell.template get<"dfs">();
				typename saw::native_data_type<sch::T>::type rho;
				std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel;
				compute_rho_u<sch::T,sch::D2Q9>(dfs,rho,vel);

				if(i > 1 && (i+2) < dim_x && j > 1 && (j+2) < dim_y){
					sum += rho;
				}
				auto angle = std::atan2(vel[1],vel[0]);
				double vel_mag = vel[1] * vel[1] + vel[0] * vel[0];
			
				auto dir_char = [&]() -> std::string_view {
					/**
					 * Flipped y due to print order
					 */
					constexpr auto pi = M_PI;
					return "■";
					if(vel_mag < 1e-4){
						return "•";
					}
					if(angle > 7.0 / 8.0 * pi){
						return "←";
					}
					if(angle > 5.0 / 8.0 * pi){
						return "↙";
					}
					if(angle > 3.0 / 8.0 * pi){
						return "↓";
					}
					if(angle > 1.0 / 8.0 * pi){
						return "↘";
					}
					if(angle > -1.0 / 8.0 * pi){
						return "→";
					}
					if(angle > -3.0 / 8.0 * pi){
						return "↗";
					}
					if(angle > -5.0 / 8.0 * pi){
						return "↑";
					}
					if(angle > -7.0 / 8.0 * pi){
						return "↖";
					}
					return "←";
				}();

				std::array<uint32_t,3u> rgb_start{64,64,255};
				std::array<uint32_t,3u> rgb_stop{255,64,64};
				std::array<uint32_t,3u> rgb_middle{255,255,255};


				auto col_interpol = [&](){
					std::array<uint32_t, 3u> rgb_interpol = rgb_start;
					double vel_mag_cut = std::min(1.0,std::max(vel_mag/(0.07*0.07),0.0));

					if(vel_mag_cut < 0.5){
						uint32_t vel_mag_i = static_cast<uint32_t>(2.0 * vel_mag_cut * 256);
						for(uint8_t i = 0u; i < 3u; ++i){
							rgb_interpol[i] = (rgb_middle[i] * vel_mag_i + (256-vel_mag_i) * rgb_start[i]) / 256;
						}
					}else{
						uint32_t vel_mag_i = static_cast<uint32_t>((2.0*(vel_mag_cut-0.5)) * 256);
						for(uint8_t i = 0u; i < 3u; ++i){
							rgb_interpol[i] = (rgb_stop[i] * vel_mag_i + (256-vel_mag_i) * rgb_middle[i]) / 256;
						}
					}
					return rgb_interpol;
				};
				auto rgb_interpol = col_interpol();

				std::cout<<"\x1b[38;2;"<<rgb_interpol[0]<<";"<<rgb_interpol[1]<<";"<<rgb_interpol[2]<<"m"<<dir_char;
				if( (i+1) < dim_x){
					std::cout<<" ";
				}else{
					std::cout<<"\n";
				}
			}, lattice);
			std::cout<<"\x1b[38;2;255;255;255m";

			/// std::cout<<"Average rho: "<<(sum / ((dim_x-4)*(dim_y-4)))<<std::endl;
			std::cout.flush();

			apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
				auto dfs = even_step ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

				auto& rho = macros.at({{i,j}}).template get<"pressure">();
				auto& vel = macros.at({{i,j}}).template get<"velocity">();
				compute_rho_u<sch::T,sch::D2Q9>(dfs,rho,vel);
			}, lattice);

			std::string vtk_f_name{"tmp/cavity_2d_"};
			vtk_f_name += std::to_string(file_no) + ".vtk";
			write_vtk_file(vtk_f_name, macros);

			++file_no;
		}
		
		lbm_step(lattice, even_step, step);

		even_step = not even_step;
	}

	/**
	 * Flush cout
	 */
	std::cout<<"\n\n";
	std::cout.flush();
	return 0;
}
