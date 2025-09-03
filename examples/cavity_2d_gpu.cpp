#include "../c++/descriptor.hpp"
#include "../c++/macroscopic.hpp"
#include "../c++/lbm.hpp"
#include "../c++/component.hpp"
#include "../c++/collision.hpp"
#include "../c++/boundary.hpp"

#include <forstio/codec/data.hpp>
// #include <forstio/remote/

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
}


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

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;
	/**
	 * Set ghost
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(0u);

	}, {{0u,0u}}, meta);

	/**
	 * Set wall
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(2u);

	}, {{0u,0u}}, meta, {{1u,1u}});

	/**
	 * Set fluid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(1u);

	}, {{0u,0u}}, meta, {{2u,2u}});

	/**
	 * Set top lid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(3u);

	}, {{0u,1u}}, {{meta.at({0u}), 2u}}, {{2u,0u}});
}

void set_initial_conditions(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::D2Q9::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,sch::D2Q9>(rho, vel);

	auto meta = latt.meta();

	/**
	 * Set distribution
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& dfs = cell.template get<"dfs">();
		auto& dfs_old = cell.template get<"dfs_old">();

		for(saw::data<sch::UInt64> k = 0; k < saw::data<sch::UInt64>{sch::D2Q9::Q}; ++k){
			dfs(k) = eq.at(k);
			dfs_old(k) = eq.at(k);
		}

	}, {{0u,0u}}, meta);
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
	uint64_t time_step
){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,sch::D2Q9>;

  /**
	* 1. Relaxation parameter \tau
	*/
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.59};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	component<sch::T, sch::D2Q9, cmpt::MovingWall> bb_lid;
	bb_lid.lid_vel = {0.1,0.0};

	auto meta = latt.meta();

	/**
	 * Collision
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		auto& dfs = cell.template get<"dfs">();
		auto& dfs_old = cell.template get<"dfs_old">();

		switch(info({0u}).get()){
			case 1u: {
				coll.apply(latt, index, time_step);
				break;
			}
			case 2u: {
				bb.apply(latt, index, time_step);
				break;
			}
			default:
				break;
		}

	}, {{0u,0u}}, meta);

	// Stream
	for(uint64_t i = 1u; (i+1u) < latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1u; (j+1u) < latt.template get_dim_size<1>().get(); ++j){
			auto& cell = latt({{i,j}});
			auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
			auto& info_new = cell.template get<"info">();

			if(info_new({0u}).get() > 0u && info_new({0u}).get() != 3u){
				for(uint64_t k = 0u; k < sch::D2Q9::Q; ++k){
					auto dir = dfi::directions[dfi::opposite_index[k]];
					auto& cell_dir_old = latt({{i+dir[0],j+dir[1]}});

					auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();
					auto& info_old = cell_dir_old.template get<"info">();

					if( info_old({0}).get() == 3u ){
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

	saw::remote<saw::rmt::Sycl> sycl_rmt;

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

	uint64_t lattice_steps = 512000u;
	bool even_step = true;

	uint64_t print_every = 256u;
	uint64_t file_no = 0u;

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{dim};

	for(uint64_t i = 0u; i < 256u; ++i){
		{
			std::string vtk_f_name{"tmp/poiseulle_2d_"};
			vtk_f_name += std::to_string(i) + ".vtk";
			write_vtk_file(vtk_f_name, macros);
		}

		// lbm_step(lattice, i);
	}
	return 0;
}
