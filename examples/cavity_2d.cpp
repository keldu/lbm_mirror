#include "../c++/descriptor.hpp"
#include "../c++/macroscopic.hpp"

/**
 */
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
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">
>;


using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;
}

/**
 * Unsure if feasible. I would have a rho normalization on the dfs and then I would use the const rho_computation
 */
template<typename Desc>
void compute_const_rho_u (
		saw::data<sch::DfCell<Desc>>& dfs,
		const typename saw::native_data_type<sch::T>::type rho,
		std::array<typename saw::native_data_type<sch::T>::type, 2>& vel
	)
{
	using dfi = df_info<sch::T, Desc>;

	saw::native_data_type<sch::T>::type real_rho = 0;
	std::fill(vel.begin(), vel.end(), 0);

	for(size_t i = 0; i < Desc::Q; ++i){
		real_rho += dfs(i).get();
		vel[0] += dfi::directions[i][0] * dfs(i).get();
		vel[1] += dfi::directions[i][1] * dfs(i).get();
	}
	for(size_t i = 0; i < Desc::Q; ++i){
		dfs(i).set(dfs(i).get() * (rho/real_rho));
	}

	vel[0] *= real_rho / (rho*rho);
	vel[1] *= real_rho / (rho*rho);
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
			1.0
			+ vel_c_cs2
			- dfi::inv_cs2 * 0.5 * ( vel[0] * vel[0] + vel[1] * vel[1] )
			+ vel_c_cs2 * vel_c_cs2 * 0.5
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
struct MovingWall {};
struct BGK {};
struct ConstRhoBGK {};
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
template<typename Desc>
class component<cmpt::MovingWall, Desc> {
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

template<typename Desc>
class component<cmpt::BGK, Desc> {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
public:
	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		typename saw::native_data_type<sch::T>::type rho;
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
		compute_rho_u<Desc>(dfs,rho,vel);
		auto eq = equilibrium<Desc>(rho,vel);
		
		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs({i}).set(dfs({i}).get() + (1.0 / relaxation_) * (eq[i] - dfs({i}).get()));
		}
	}
};

template<typename Desc>
class component<cmpt::ConstRhoBGK, Desc> {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
	typename saw::native_data_type<sch::T>::type rho_;
public:
	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
		compute_const_rho_u<Desc>(dfs,rho_,vel);
		auto eq = equilibrium<Desc>(rho_,vel);
		
		for(uint64_t i = 0u; i < Desc::Q; ++i){
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
	bool even_step
){
	using namespace kel::lbm;
  using dfi = df_info<sch::T,sch::D2Q9>;

	component<cmpt::BGK,sch::D2Q9> coll;
	coll.relaxation_ = 0.5384;
	component<cmpt::ConstRhoBGK,sch::D2Q9> rho_coll;
	rho_coll.relaxation_ = 0.5384;
	rho_coll.rho_ = 1.0;

	component<cmpt::BounceBack,sch::D2Q9> bb;
	component<cmpt::MovingWall,sch::D2Q9> bb_lid;
	bb_lid.lid_vel = {0.1,0.0};

	// Collide
	apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
		auto& df = even_step ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
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
	}, latt);

	// Stream
	for(uint64_t i = 1; (i+1) < latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1; (j+1) < latt.template get_dim_size<1>().get(); ++j){
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

	// auto& df_field = lattices.at(0).template get<"dfs">();
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

	for(uint64_t step = 0; step < lattice_steps; ++step){

		if(step % print_every == 0u){
			std::cout<<"\n";
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

			std::ofstream vtk_file{"tmp/cavity_2d_"+std::to_string(file_no)+".vtk"};

			vtk_file <<"# vtk DataFile Version 3.0\n";
			vtk_file <<"Velocity Cavity2D example\n";
			vtk_file <<"ASCII\n";
			vtk_file <<"DATASET STRUCTURED_POINTS\n";
			vtk_file <<"DIMENSIONS "<< dim_x <<" "<<dim_y<<" 1\n";
			vtk_file <<"SPACING 1.0 1.0 1.0\n";
			vtk_file <<"ORIGIN 0.0 0.0 0.0\n";
			vtk_file <<"POINT_DATA "<<(dim_x*dim_y)<<"\n";
			vtk_file <<"VECTORS Velocity float\n";
			
			apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
				auto dfs = even_step ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

				typename saw::native_data_type<sch::T>::type rho;
				std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel;
				compute_rho_u<sch::D2Q9>(dfs,rho,vel);

				vtk_file << static_cast<float>(vel[0u]) << " " << static_cast<float>(vel[1u])<<" 0.0\n";
			}, lattice);
			

			++file_no;
		}
		
		lbm_step(lattice, even_step);

		even_step = !even_step;
	}

	/**
	 * Flush cout
	 */
	std::cout<<"\n\n";
	std::cout.flush();
	return 0;
}
