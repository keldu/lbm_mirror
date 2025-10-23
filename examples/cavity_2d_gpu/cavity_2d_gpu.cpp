#include <kel/lbm/lbm.hpp>

#include <forstio/codec/data.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>

#include <iostream>
#include <fstream>
#include <cmath>

namespace saw {
namespace encode {
template<typename InnerEnc>
struct Sycl {
};
}

template<typename Schema>
class data<Schema, encode::Sycl<encode::Native>> {
private:
	acpp::sycl::buffer<data<Schema, encode::Native>> data_;
	data<schema::UInt64, encode::Native> size_;
public:
	data(const data<Schema, encode::Native>& data__):
		data_{&data__, 1u},
		size_{data__.size()}
	{}

	auto& get_handle() {
		return data_;
	}

	const auto& get_handle() const {
		return data_;
	}

	data<schema::UInt64, encode::Native> size() const {
		return size_;
	}

	template<acpp::sycl::access::mode AccessMode>
	auto access(acpp::sycl::handler& h){
		return data_.template get_access<AccessMode>(h);
	}

	template<acpp::sycl::access::mode AccessMode>
	auto access(acpp::sycl::handler& h) const {
		return data_.template get_access<AccessMode>(h);
	}
};

template<typename Sch, uint64_t Dim>
class data<schema::Array<Sch, Dim>, encode::Sycl<encode::Native>> {
public:
	using Schema = schema::Array<Sch,Dim>;
private:
	acpp::sycl::buffer<data<Sch, encode::Native>> data_;
	data<schema::UInt64, encode::Native> size_;
public:
	data(const data<Schema, encode::Native>& host_data__):
		data_{&host_data__.at({0u}),host_data__.size().get()},
		size_{host_data__.size()}
	{}

	auto& get_handle() {
		return data_;
	}

	const auto& get_handle() const {
		return data_;
	}

	data<schema::UInt64, encode::Native> size() const {
		return size_;
	}

	template<acpp::sycl::access::mode AccessMode>
	auto access(acpp::sycl::handler& h){
		return data_.template get_access<AccessMode>(h);
	}

	template<acpp::sycl::access::mode AccessMode>
	auto access(acpp::sycl::handler& h) const {
		return data_.template get_access<AccessMode>(h);
	}
};
}

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

void set_geometry(saw::data<sch::CavityFieldD2Q9>& lattice){
	using namespace kel::lbm;
	auto meta = lattice.meta();
	auto& info_field = lattice.template get<"info">();
	/**
	 * Set ghost
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& info = info_field.at(index);

		info({0u}).set(0u);

	}, {{0u,0u}}, meta);

	/**
	 * Set wall
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& info = info_field.at(index);

		info({0u}).set(2u);

	}, {{0u,0u}}, meta, {{1u,1u}});

	/**
	 * Set fluid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& info = info_field.at(index);

		info({0u}).set(1u);

	}, {{0u,0u}}, meta, {{2u,2u}});

	/**
	 * Set top lid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& info = info_field.at(index);
		info({0u}).set(3u);

	}, {{0u,1u}}, {{meta.at({0u}), 2u}}, {{2u,0u}});
}

void set_initial_conditions(
	saw::data<sch::CavityFieldD2Q9>& lattice
){
	using namespace kel::lbm;

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::D2Q9::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,sch::D2Q9>(rho, vel);

	auto meta = lattice.meta();
	auto& dfs_field = lattice.template get<"dfs">();
	auto& dfs_old_field = lattice.template get<"dfs_old">();

	/**
	 * Set distribution
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& dfs = dfs_field.at(index);
		auto& dfs_old = dfs_old_field.at(index);

		for(saw::data<sch::UInt64> k = 0; k < saw::data<sch::UInt64>{sch::D2Q9::Q}; ++k){
			dfs(k) = eq.at(k);
			dfs_old(k) = eq.at(k);
		}

	}, {{0u,0u}}, meta);
}

template<typename T, typename Desc>
void lbm_step(
	acpp::sycl::buffer<saw::data<sch::CellInfo<Desc>>>& info,
	acpp::sycl::buffer<saw::data<sch::DfCell<Desc>>>& dfs,
	acpp::sycl::buffer<saw::data<sch::DfCell<Desc>>>& dfs_old,
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

	/*
	const size_t Nx = latt.template get_dim_size<0>().get();
	const size_t Ny = latt.template get_dim_size<1>().get();
	*/

	// Submit collision kernel
	sycl_q.submit([&](sycl::handler& cgh) {
	 // Accessor for latt with read/write
	 auto info_acc = info.template get_access<sycl::access::mode::read>(cgh);
	 auto dfs_acc = dfs.template get_access<sycl::access::mode::read_write>(cgh);
	 auto dfs_old_acc = dfs_old.template get_access<sycl::access::mode::read_write>(cgh);

	 cgh.parallel_for(
		sycl::range<2>{dim_x, dim_y}, [=](sycl::id<2> idx) {
			size_t i = idx[0];
			size_t j = idx[1];

			size_t acc_id = i * dim_x + j;

			// Read cell info
			auto& info = info_acc[acc_id];

			switch (info({0u}).get()) {
			 case 1u: {
				// bb.apply(latt_acc, {i, j}, time_step);

						auto& dfs_old = (is_even) ? dfs_old_acc[acc_id] : dfs_acc[acc_id];
						// auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
						auto df_cpy = dfs_old.copy();

						for(uint64_t i = 1u; i < Desc::Q; ++i){
							dfs_old({i}) = df_cpy({dfi::opposite_index.at(i)});
						}

				break;
			 }
			 case 2u: {
				// coll.apply(latt_acc, {i, j}, time_step);

						auto& dfs_old = (is_even) ? dfs_old_acc[acc_id] : dfs_acc[acc_id];

						saw::data<T> rho;
						saw::data<sch::FixedArray<T,Desc::D>> vel;
						compute_rho_u<T,Desc>(dfs_old,rho,vel);
						auto eq = equilibrium<T,Desc>(rho,vel);

						for(uint64_t i = 0u; i < Desc::Q; ++i){
							dfs_old({i}) = dfs_old({i}) + frequency * (eq.at(i) - dfs_old({i}));
							// dfs_old({i}).set(dfs_old({i}).get() + (1.0 / relaxation_) * (eq.at(i).get() - dfs_old({i}).get()));
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
	 auto info_acc = info.template get_access<sycl::access::mode::read_write>(cgh);
	 auto dfs_new_acc = is_even ? dfs.template get_access<sycl::access::mode::write>(cgh) : dfs_old.template get_access<sycl::access::mode::write>(cgh);
	 auto dfs_old_acc = is_even ? dfs_old.template get_access<sycl::access::mode::read>(cgh) : dfs.template get_access<sycl::access::mode::read>(cgh);

	 cgh.parallel_for(
		sycl::range<2>{dim_x - 2, dim_y - 2}, [=](sycl::id<2> idx) {
			size_t i = idx[0] + 1; // avoid boundary
			size_t j = idx[1] + 1;

			size_t acc_id = i * dim_x + j;

			auto dfs_new = dfs_new_acc[acc_id];
			auto& info = info_acc[acc_id];

			if (info({0u}).get() > 0u && info({0u}).get() != 3u) {
			 for (uint64_t k = 0u; k < Desc::Q; ++k) {
				auto dir = dfi::directions[dfi::opposite_index[k]];
				// auto& cell_dir_old = latt_acc({i + dir[0], j + dir[1]});
				//
				size_t acc_old_id = (i+dir[0]) * dim_x + (j+dir[1]);

				auto& dfs_old = dfs_old_acc[acc_old_id];
				auto& info_old = info_acc[acc_old_id];

				if (info_old({0}).get() == 3u) {
					auto& dfs_old_loc = dfs_old_acc[acc_old_id];
					dfs_new({k}) = dfs_old_loc({dfi::opposite_index.at(k)}) - 2.0 * dfi::inv_cs2 * dfi::weights.at(k) * 1.0 * (bb_lid.lid_vel[0] * dir[0] + bb_lid.lid_vel[1] * dir[1]);
				} else {
					dfs_new({k}) = dfs_old({k});
				}
			 }
			}
		});
	});

	sycl_q.wait();
}
}
}

int main(){
	using namespace kel::lbm;

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{dim_x, dim_y}};
	constexpr size_t dim_size = dim_x * dim_y;

	saw::data<sch::CavityFieldD2Q9> lattice{dim};

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

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(lattice);

	/**
	 *
	 */
	set_initial_conditions(lattice);

	auto& info_field = lattice.template get<"info">();
	auto& dfs_field = lattice.template get<"dfs">();
	auto& dfs_old_field = lattice.template get<"dfs_old">();

	acpp::sycl::queue sycl_q{acpp::sycl::default_selector_v, acpp::sycl::property::queue::in_order{}};
	acpp::sycl::buffer<saw::data<sch::CellInfo<sch::D2Q9>>> info_sycl{info_field.internal_data(), info_field.internal_size().get()};
	acpp::sycl::buffer<saw::data<sch::DfCell<sch::D2Q9>>> dfs_sycl{dfs_field.internal_data(), dfs_field.internal_size().get()};
	acpp::sycl::buffer<saw::data<sch::DfCell<sch::D2Q9>>> dfs_old_sycl{dfs_old_field.internal_data(), dfs_old_field.internal_size().get()};

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

		lbm_step<sch::T,sch::D2Q9>(info_sycl, dfs_sycl, dfs_old_sycl, (i%2u == 0u), i, sycl_q);
	}
	return 0;
}
