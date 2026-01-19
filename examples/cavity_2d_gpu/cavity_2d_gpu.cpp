#include <kel/lbm/lbm.hpp>

#include <forstio/codec/data.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
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
using D2Q9 = Descriptor<2u,9u>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0u, 0u, 1u>;

template<typename Desc>
using CellInfo = Cell<UInt8, Desc, 1u, 0u, 0u>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = CellFieldStruct<
	Desc,
	Struct<
		Member<CellField<Desc,DfCell<Desc>>, "dfs">,
		Member<CellField<Desc,DfCell<Desc>>, "dfs_old">,
		Member<CellField<Desc,CellInfo<Desc>>, "info">
	>
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">
>;

using CavityFieldD2Q9 = CellStruct<D2Q9>;
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

constexpr size_t dim_x = 128;
constexpr size_t dim_y = 128;

template<typename Desc>
void set_geometry(
	saw::data<sch::CellInfo<Desc>>* info_field
){
	using namespace kel::lbm;
	using namespace acpp;
	saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	/**
	 * Set ghost
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		size_t i = index.at({0u}).get() * dim_x + index.at({1u}).get();
		auto& info = info_field[i];

		info({0u}).set(0u);

	}, {{0u,0u}}, meta);

	/**
	 * Set wall
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		size_t i = index.at({0u}).get() * dim_x + index.at({1u}).get();
		auto& info = info_field[i];

		info({0u}).set(1u);

	}, {{0u,0u}}, meta, {{1u,1u}});

	/**
	 * Set fluid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		size_t i = index.at({0u}).get() * dim_x + index.at({1u}).get();
		auto& info = info_field[i];

		info({0u}).set(2u);

	}, {{0u,0u}}, meta, {{2u,2u}});

	/**
	 * Set top lid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		size_t i = index.at({0u}).get() * dim_x + index.at({1u}).get();
		auto& info = info_field[i];
		info({0u}).set(3u);

	}, {{0u,1u}}, {{meta.at({0u}), 2u}}, {{2u,0u}});
}

template<typename Desc>
void set_initial_conditions(
	saw::data<sch::CellInfo<Desc>>* info_field,
	saw::data<sch::DfCell<Desc>>* dfs_field,
	saw::data<sch::DfCell<Desc>>* dfs_old_field
){
	using namespace kel::lbm;
	using namespace acpp;

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,Desc::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,Desc>(rho, vel);
	saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};

	/**
	 * Set distribution
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& index){
		size_t i = index.at({0u}).get() * dim_x + index.at({1u}).get();
		auto& dfs = dfs_field[i];
		auto& dfs_old = dfs_old_field[i];

		for(saw::data<sch::UInt64> k = 0; k < saw::data<sch::UInt64>{Desc::Q}; ++k){
			dfs(k) = eq.at(k);
			dfs_old(k) = eq.at(k);
		}

	}, {{0u,0u}}, meta);
}

template<typename T, typename Desc>
void lbm_step(
	saw::data<sch::CellInfo<Desc>>* info_r,
	saw::data<sch::DfCell<Desc>>* dfs_r,
	saw::data<sch::DfCell<Desc>>* dfs_r_old,
	bool is_even,
	uint64_t time_step,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;
	using namespace acpp;

	using dfi = df_info<T,Desc>;

	//component<T, Desc, cmpt::BGK> coll{0.59};
	constexpr saw::data<T> frequency{1.0 / 0.59};
	//component<T, Desc, cmpt::BounceBack> bb;
	component<T, Desc, cmpt::MovingWall> bb_lid;
	bb_lid.lid_vel = {0.1, 0.0};

	// Submit collision kernel
	sycl_q.submit([&](sycl::handler& cgh) {
	 // Accessor for latt with read/write

	 cgh.parallel_for(
		sycl::range<2>{dim_x, dim_y}, [=](sycl::id<2> idx) {
			size_t i = idx[0];
			size_t j = idx[1];

			size_t acc_id = i * dim_x + j;

			// Read cell info
			auto& info = info_r[acc_id];

			switch (info({0u}).get()) {
			 case 1u: {
				// bb.apply(latt_acc, {i, j}, time_step);

				auto& dfs_old = is_even ? dfs_r_old[acc_id] : dfs_r[acc_id];
				auto df_cpy = dfs_old.copy();

				for(uint64_t i = 1u; i < Desc::Q; ++i){
					dfs_old({i}) = df_cpy({dfi::opposite_index.at(i)});
				}

				break;
			 }
			 case 2u: {
				// coll.apply(latt_acc, {i, j}, time_step);
				auto& dfs_old = is_even ? dfs_r_old[acc_id] : dfs_r[acc_id];

				saw::data<T> rho;
				saw::data<sch::FixedArray<T,Desc::D>> vel;
				compute_rho_u<T,Desc>(dfs_old,rho,vel);
				auto eq = equilibrium<T,Desc>(rho,vel);

				for(uint64_t i = 0u; i < Desc::Q; ++i){
					dfs_old({i}) = dfs_old({i}) + frequency * (eq.at(i) - dfs_old({i}));
				}
				break;
			}
			 default:
				// Do nothing
				break;
			}
		});
	});

	// Submit streaming kernel
	sycl_q.submit([&](sycl::handler& cgh) {

	 cgh.parallel_for(
		sycl::range<2>{dim_x - 2, dim_y - 2}, [=](sycl::id<2> idx) {
			size_t i = idx[0] + 1; // avoid boundary
			size_t j = idx[1] + 1;

			size_t acc_id = i * dim_x + j;

			auto& dfs_new = is_even ? dfs_r[acc_id] : dfs_r_old[acc_id];
			auto& info = info_r[acc_id];


			if (info({0u}).get() > 0u && info({0u}).get() != 3u) {
				for (uint64_t k = 0u; k < Desc::Q; ++k) {
					auto dir = dfi::directions[dfi::opposite_index[k]];
					// auto& cell_dir_old = latt_acc({i + dir[0], j + dir[1]});
					//
					size_t acc_old_id = (i+dir[0]) * dim_x + (j+dir[1]);

					auto& dfs_old = is_even ? dfs_r_old[acc_old_id] : dfs_r[acc_old_id];
					auto& info_old = info_r[acc_old_id];

					if (info_old({0}).get() == 3u) {
						auto& dfs_old_loc = is_even ? dfs_r_old[acc_id] : dfs_r[acc_id];
						dfs_new({k}) = dfs_old_loc({dfi::opposite_index.at(k)}) - 2.0 * dfi::inv_cs2 * dfi::weights.at(k) * 1.0 * (bb_lid.lid_vel[0] * dir[0] + bb_lid.lid_vel[1] * dir[1]);
					} else {
						dfs_new({k}) = dfs_old({k});
					}
				}
			}
		});
	});
}
}
}

int main(){
	using namespace kel::lbm;
	using namespace acpp;

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> meta{{dim_x, dim_y}};
	constexpr size_t dim_size = dim_x * dim_y;

	converter<sch::T> conv{
		{0.1},
		{0.1}
	};

	print_lbm_meta<sch::T, sch::D2Q9>(conv, {1e-3});

	auto eo_lbm_dir = output_directory();
	if(eo_lbm_dir.is_error()){
		return -1;
	}
	auto& lbm_dir = eo_lbm_dir.get_value();
	auto out_dir = lbm_dir / "cavity_gpu_2d";

	sycl::queue sycl_q{sycl::default_selector_v, sycl::property::queue::in_order{}};

	constexpr size_t num_cells = dim_x * dim_y;

	// Allocate USM shared memory for your main data
	auto* info     = sycl::malloc_shared<saw::data<sch::CellInfo<sch::D2Q9>>>(num_cells, sycl_q);
	auto* dfs      = sycl::malloc_shared<saw::data<sch::DfCell<sch::D2Q9>>>(num_cells, sycl_q);
	auto* dfs_old  = sycl::malloc_shared<saw::data<sch::DfCell<sch::D2Q9>>>(num_cells, sycl_q);

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(info);

	/**
	 *
	 */
	set_initial_conditions(info,dfs,dfs_old);

	/**
	 * Timeloop
	 */
	uint64_t lattice_steps = 512000u;
	bool even_step = true;

	uint64_t print_every = 256u;
	uint64_t file_no = 0u;

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{meta};

	std::cout<<"Start"<<std::endl;

	auto start = std::chrono::steady_clock::now();
	sycl_q.wait();
	for(uint64_t i = 0u; i < lattice_steps; ++i){
		lbm_step<sch::T,sch::D2Q9>(info, dfs, dfs_old, even_step, i, sycl_q);
		if(i % 1024u == 0u){
			sycl_q.wait();
			iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>>& index){
				size_t j = index.at({0u}).get() * dim_x + index.at({1u}).get();
				auto dfs_field = even_step ? dfs_old : dfs;

				auto& rho = macros.at(index).template get<"pressure">();
				auto& vel = macros.at(index).template get<"velocity">();
				compute_rho_u<sch::T,sch::D2Q9>(dfs_field[j],rho,vel);
			}, {{0u,0u}}, meta);
			std::string vtk_f_name{"tmp/cavity_2d_gpu_"};
			vtk_f_name += std::to_string(i) + ".vtk";
			write_vtk_file(vtk_f_name, macros);
		}

		even_step = not even_step;
	}
	auto stop = std::chrono::steady_clock::now();
	std::cout<<std::format("{:%H:%M:%S}",(stop-start))<<std::endl;

	sycl_q.wait();
	sycl::free(info,sycl_q);
	sycl::free(dfs,sycl_q);
	sycl::free(dfs_old,sycl_q);
	return 0;
}_
