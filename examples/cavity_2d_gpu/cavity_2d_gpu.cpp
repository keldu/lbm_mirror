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
	cl::sycl::buffer<data<Schema, encode::Native>> data_;
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

	template<cl::sycl::access::mode AccessMode>
	auto access(cl::sycl::handler& h){
		return data_.template get_access<AccessMode>(h);
	}

	template<cl::sycl::access::mode AccessMode>
	auto access(cl::sycl::handler& h) const {
		return data_.template get_access<AccessMode>(h);
	}
};

template<typename Sch, uint64_t Dim>
class data<schema::Array<Sch, Dim>, encode::Sycl<encode::Native>> {
public:
	using Schema = schema::Array<Sch,Dim>;
private:
	cl::sycl::buffer<data<Sch, encode::Native>> data_;
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

	template<cl::sycl::access::mode AccessMode>
	auto access(cl::sycl::handler& h){
		return data_.template get_access<AccessMode>(h);
	}

	template<cl::sycl::access::mode AccessMode>
	auto access(cl::sycl::handler& h) const {
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

constexpr size_t dim_size = 2;
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

struct sycl_component_context {
	acpp::sycl::handler* handler = nullptr;
};

void lbm_step(
	saw::data<sch::CellField<sch::D2Q9,sch::CellInfo<sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>>& info,
	saw::data<sch::CellField<sch::D2Q9,kel::lbm::sch::DfCell  <sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>>& dfs,
	saw::data<sch::CellField<sch::D2Q9,kel::lbm::sch::DfCell  <sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>>& dfs_old,
	bool even_step,
	uint64_t time_step,
	acpp::sycl::queue& sycl_q
){
  using namespace kel::lbm;
  using namespace acpp;

  using dfi = df_info<sch::T, sch::D2Q9>;

  //component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.59};
  //component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
  component<sch::T, sch::D2Q9, cmpt::MovingWall> bb_lid;
  bb_lid.lid_vel = {0.1, 0.0};

  const size_t Nx = latt.template get_dim_size<0>().get();
  const size_t Ny = latt.template get_dim_size<1>().get();

  // Submit collision kernel
  sycl_q.submit([&](sycl::handler& cgh) {
    // Accessor for latt with read/write
    auto info_acc = info.template get_access<sycl::access::mode::read>(cgh);
    auto dfs_acc = dfs.template get_access<sycl::access::mode::read_write>(cgh);
    auto dfs_old_acc = dfs_old.template get_access<sycl::access::mode::read_write>(cgh);

    cgh.parallel_for(
      sycl::range<2>{Nx, Ny}, [=](sycl::id<2> idx) {
        size_t i = idx[0];
        size_t j = idx[1];

        // Read cell info
        auto& info = info_acc[idx];

        switch (info({0u}).get()) {
          case 1u: {
            // bb.apply(latt_acc, {i, j}, time_step);
				// bool is_even = ((time_step % 2) == 0);

				auto& dfs_old = (is_even) ? dfs_old_acc[idx] : dfs_acc[idx];
				// auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

				saw::data<T> rho;
				saw::data<sch::FixedArray<T,Descriptor::D>> vel;
				compute_rho_u<T,Descriptor>(dfs_old,rho,vel);
				auto eq = equilibrium<T,Descriptor>(rho,vel);

				for(uint64_t i = 0u; i < Descriptor::Q; ++i){
					dfs_old({i}) = dfs_old({i}) + frequency_ * (eq.at(i) - dfs_old({i}));
					// dfs_old({i}).set(dfs_old({i}).get() + (1.0 / relaxation_) * (eq.at(i).get() - dfs_old({i}).get()));
				}
            break;
          }
          case 2u: {
            // coll.apply(latt_acc, {i, j}, time_step);
				// bool is_even = ((time_step % 2) == 0);

				auto& dfs_old = (is_even) ? dfs_old_acc[idx] : dfs_acc[idx];
				// auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

				saw::data<T> rho;
				saw::data<sch::FixedArray<T,Descriptor::D>> vel;
				compute_rho_u<T,Descriptor>(dfs_old,rho,vel);
				auto eq = equilibrium<T,Descriptor>(rho,vel);

				using dfi = df_info<T,Descriptor>;

				auto& force = cell.template get<"force">();

				for(uint64_t i = 0u; i < Descriptor::Q; ++i){
					// saw::data<T> ci_min_u{0};
					saw::data<T> ci_dot_u{0};
					for(uint64_t d = 0u; d < Descriptor::D; ++d){
						ci_dot_u = ci_dot_u + vel.at({d}) * saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])};
					}
					saw::data<T> F_i{0};// = saw::data<T>{dfi::weights[i]} * (ci_dot_F * dfi::inv_cs2 + ci_dot_u * ci_dot_F * (dfi::inv_cs2 * dfi::inv_cs2));

					for(uint64_t d = 0u; d < Descriptor::D; ++d){
						F_i = F_i +
							saw::data<T>{dfi::weights[i]} * ((saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])}
							- vel.at({d}) ) * dfi::inv_cs2 + ci_dot_u * saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])} * dfi::inv_cs2  * dfi::inv_cs2 ) * force({d});
					}


					dfs_old({i}) = dfs_old({i}) + frequency_ * (eq.at(i) - dfs_old({i}) ) + F_i * (saw::data<T>{1} - saw::data<T>{0.5f} * frequency_);
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

    cgh.parallel_for(
      sycl::range<2>{Nx - 2, Ny - 2}, [=](sycl::id<2> idx) {
        size_t i = idx[0] + 1; // avoid boundary
        size_t j = idx[1] + 1;

        auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
        auto& info_new = cell.template get<"info">();

        if (info_new({0u}).get() > 0u && info_new({0u}).get() != 3u) {
          for (uint64_t k = 0u; k < sch::D2Q9::Q; ++k) {
            auto dir = dfi::directions[dfi::opposite_index[k]];
            auto& cell_dir_old = latt_acc({i + dir[0], j + dir[1]});

            auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();
            auto& info_old = cell_dir_old.template get<"info">();

            if (info_old({0}).get() == 3u) {
              auto& df_old_loc = even_step ? latt_acc({i, j}).template get<"dfs_old">() : latt_acc({i, j}).template get<"dfs">();
              df_new({k}) = df_old_loc({dfi::opposite_index.at(k)}) - 2.0 * dfi::inv_cs2 * dfi::weights.at(k) * 1.0 * (bb_lid.lid_vel[0] * dir[0] + bb_lid.lid_vel[1] * dir[1]);
            } else {
              df_new({k}) = df_old({k});
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

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{dim_x,info_field.at dim_y}};
	const dim_size = dim_x * dim_y;

	saw::data<sch::CavityFieldD2Q9>> lattice{dim};

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
	saw::data<sch::CellField<Desc,sch::CellInfo<sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>> sycl_info_field{info_field};
	saw::data<sch::CellField<Desc,sch::DfCell<sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>> sycl_dfs_field{dfs_field};
	saw::data<sch::CellField<Desc,sch::DfCell<sch::D2Q9>>, saw::encode::Sycl<saw::encode::Native>> sycl_dfs_old_field{dfs_old_field};

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

		lbm_step(sycl_info_field, sycl_dfs_field, sycl_dfs_old_field, (i%2u == 0u), i, sycl_q);
	}
	return 0;
}
