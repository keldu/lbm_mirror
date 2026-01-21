#include <kel/lbm/lbm.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>

#include <forstio/codec/data.hpp>

template<typename T>
using SyclHostAlloc = acpp::sycl::usm_allocator<saw::data<T>, acpp::sycl::usm::alloc::host>;

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
using T = Float64;
using D2Q9 = Descriptor<2u,9u>;

using DfCell = Cell<T, D2Q9, 0u, 0u, 1u>;
using CellInfo = Cell<UInt8, D2Q9, 1u, 0u, 0u>;

/**
 * Basic type for simulation
 */
using CellStruct = Struct<
	Member<DfCell, "dfs">,
	Member<DfCell, "dfs_old">,
	Member<CellInfo, "info">
>;

using MacroStruct = Struct<
	Member<FixedArray<Float64,D2Q9::D>, "velocity">,
	Member<Float64, "pressure">
>;

}

namespace cmpt {
template<bool East>
struct PressureBoundaryRestrictedVelocityTo {};
}

template<typename FP,typename Desc, bool East>
struct component<FP,Desc, cmpt::PressureBoundaryRestrictedVelocityTo<East>> {
private:
	saw::data<FP> pressure_setting_;
	saw::data<FP> rho_setting_;
public:
	component(const saw::data<FP>& pressure_setting__):
		pressure_setting_{pressure_setting__},
		rho_setting_{pressure_setting__ * df_info<FP,Desc>::inv_cs2}
	{}

	template<typename CellFieldSchema>
	void apply(
			saw::data<CellFieldSchema>* field,
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index,
			const saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& meta,
			uint64_t time_step
		) const {

		using dfi = df_info<FP,Desc>;

		auto flat_ind = flatten_index<sch::UInt64,Desc::D>::apply(index,meta);

		bool is_even = ((time_step % 2u) == 0u);
		auto& cell = field[flat_ind.get()];

		auto& info = cell.template get<"info">();
		if(info({0u}).get() == 0u){
			return;
		}
		auto& dfs_old = (is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		/**
		 * Sum all known DFs
		 */
		saw::data<FP> sum_df{0u};
		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Desc::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			auto flat_ind_n = flatten_index<sch::UInt64,Desc::D>::apply({{index.at({0u})+c_k[0u], index.at({1u})+c_k[1u]}},meta);
			auto& cell_n = field[flat_ind_n.get()];
			auto& info_n = cell_n.template get<"info">();
			auto info_n_val = info_n({0u});
			auto k_opp = dfi::opposite_index[k.get()];

			if(info_n_val.get() > 0u){
				sum_df += dfs_old({k_opp});
			}
		}
		/**
		 * Get the sum of the unknown dfs and precalculate the direction
		 */
		constexpr int known_dir = East ? 1 : -1;
		auto sum_unknown_dfs = (rho_setting_ - sum_df) * saw::data<FP>{known_dir};

		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Desc::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			auto flat_ind_n = flatten_index<sch::UInt64,Desc::D>::apply({{index.at({0u})+c_k[0u], index.at({1u})+c_k[1u]}},meta);
			auto& cell_n = field[flat_ind_n.get()];
			auto& info_n = cell_n.template get<"info">();
			auto info_n_val = info_n({0u});

			if(info_n_val.get() > 0u){
				sum_unknown_dfs += dfs_old({k}) * c_k[0u];
			}
		}

		auto vel_x = sum_unknown_dfs / rho_setting_;

		if constexpr (East) {
			dfs_old({2u}) = dfs_old({1u}) + saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({6u}) = dfs_old({5u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
			dfs_old({8u}) = dfs_old({7u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
		}else if constexpr (not East){
			dfs_old({1u}) = dfs_old({2u}) - saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({5u}) = dfs_old({6u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
			dfs_old({7u}) = dfs_old({8u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
		}
	}
};

/**
 * This is massively hacky and expects a lot of conditions
 * Either this or mirrored along the horizontal line works
 *
 * 0 - 2 - 2
 * 0 - 3 - 1
 * 0 - 3 - 1
 * .........
 * 0 - 3 - 1
 * 0 - 2 - 2
 *
 */

template<typename Desc>
saw::error_or<void> set_geometry(
	saw::data<sch::CellStruct>* cells,
	const saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& meta,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;
	
	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,Desc::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,Desc>(rho, vel);

	sycl_q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<2>{meta.at({0}).get(), meta.at({1}).get()},[=](acpp::sycl::id<2> idx){
			size_t i = idx[0];
			size_t j = idx[1];
			size_t acc_id = j * meta.at({0u}).get() + i;

			auto& c = cells[acc_id];
			auto& info = c.template get<"info">()({0});
			auto& dfs = c.template get<"dfs">();
			auto& dfs_old = c.template get<"dfs_old">();

			if(i >= 2u and j >= 2u and (i+2u) < meta.at({0u}).get() and (j+2u) < meta.at({1u}).get()){
				// Fluid
				info.set({2u});
			}else if(((j+2u) == meta.at({1u}).get() or j == 1u) and (i>=1u and (i+1u)<meta.at({0u}).get() )){
				// Wall
				info.set({1u});
			}else if((i==1u) and (j >= 1 and (j+1 < meta.at({1u}).get()) ) ){
				// Left input
				info.set({3u});
			}else if((i+2u) == meta.at({0u}).get() and (j >= 1 and (j+1) < meta.at({1u}).get() )){
				// Right output
				info.set({4u});
			}else {
				info.set({0u});
			}
			for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Desc::Q}; ++k){
				dfs(k) = eq.at(k);
				dfs_old(k) = eq.at(k);
			}
		});
	}).wait();

	return saw::make_void();
}

template<typename Desc>
void step(
	saw::data<sch::CellStruct>* cells,
	saw::data<sch::MacroStruct>* macro_cells,
	const saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& meta,
	uint64_t time_step,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,Desc>;
	
	constexpr saw::data<sch::T> frequency{1.0 / 0.6};

	bool is_even = ((time_step % 2u) == 0u);
  /**
	* 1. Relaxation parameter \tau
	*/
  /*
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.5384};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	*/
	component<sch::T, Desc, cmpt::PressureBoundaryRestrictedVelocityTo<true>> inlet{1.01 * dfi::cs2 * 2.0/3.0};
	component<sch::T, Desc, cmpt::PressureBoundaryRestrictedVelocityTo<false>> outlet{1.0 * dfi::cs2 * 2.0/3.0};

			
	// auto collision_ev = 
	sycl_q.submit([&](acpp::sycl::handler& h){
		
		/// Collision
		h.parallel_for(acpp::sycl::range<2>{meta.at({0}).get(), meta.at({1}).get()},[=](acpp::sycl::id<Desc::D> idx){
			size_t i = idx[0];
			size_t j = idx[1];
			size_t acc_id = j * meta.at({0u}).get() + i;
			
			auto& c = cells[acc_id];
			auto& info = cells[acc_id].template get<"info">();

			switch (info({0u}).get()) {
				// Bounce Back
			 case 1u: {
				auto& dfs_old = is_even ? c.template get<"dfs_old">() : c.template get<"dfs">();
				auto df_cpy = dfs_old.copy();

				for(uint64_t k = 1u; k < Desc::Q; ++k){
					dfs_old({k}) = df_cpy({dfi::opposite_index.at(k)});
				}

				break;
			 }
			 // Collision
			 case 2u: {
				// coll.apply(latt_acc, {i, j}, time_step);
				auto& dfs_old = is_even ? c.template get<"dfs_old">() : c.template get<"dfs">();

				auto& macro_c = macro_cells[acc_id];

				saw::data<sch::T>& rho = macro_c.template get<"pressure">();
				saw::data<sch::FixedArray<sch::T,Desc::D>>& vel = macro_c.template get<"velocity">();

				compute_rho_u<sch::T,Desc>(dfs_old,rho,vel);
				auto eq = equilibrium<sch::T,Desc>(rho,vel);

				for(uint64_t k = 0u; k < Desc::Q; ++k){
					dfs_old({k}) = dfs_old({k}) + frequency * (eq.at({k}) - dfs_old({k}));
				}
				break;
			}
			case 3u: {
				inlet.apply(cells, {{i,j}}, meta, time_step);
				auto& dfs_old = is_even ? c.template get<"dfs_old">() : c.template get<"dfs">();

				auto& macro_c = macro_cells[acc_id];

				saw::data<sch::T>& rho = macro_c.template get<"pressure">();
				saw::data<sch::FixedArray<sch::T,Desc::D>>& vel = macro_c.template get<"velocity">();

				compute_rho_u<sch::T,Desc>(dfs_old,rho,vel);
				auto eq = equilibrium<sch::T,Desc>(rho,vel);

				for(uint64_t k = 0u; k < Desc::Q; ++k){
					dfs_old({k}) = dfs_old({k}) + frequency * (eq.at({k}) - dfs_old({k}));
				}
				break;
			}
			case 4u: {
				outlet.apply(cells, {{i,j}}, meta, time_step);
				auto& dfs_old = is_even ? c.template get<"dfs_old">() : c.template get<"dfs">();

				auto& macro_c = macro_cells[acc_id];

				saw::data<sch::T>& rho = macro_c.template get<"pressure">();
				saw::data<sch::FixedArray<sch::T,Desc::D>>& vel = macro_c.template get<"velocity">();

				compute_rho_u<sch::T,Desc>(dfs_old,rho,vel);
				auto eq = equilibrium<sch::T,Desc>(rho,vel);

				for(uint64_t k = 0u; k < Desc::Q; ++k){
					dfs_old({k}) = dfs_old({k}) + frequency * (eq.at({k}) - dfs_old({k}));
				}
				break;
			}
			default:
				// Do nothing
				break;
			}
		});
	}).wait();
	
	//auto stream_ev = 
	sycl_q.submit([&](acpp::sycl::handler& h){
		/// Stream
		h.parallel_for(acpp::sycl::range<2>{meta.at({0}).get(), meta.at({1}).get()},[=](acpp::sycl::id<Desc::D> idx){
			size_t i = idx[0];
			size_t j = idx[1];
			size_t acc_id = j * meta.at({0u}).get() + i;

			auto& c = cells[acc_id];
			auto& info = c.template get<"info">();
			auto& dfs_new = is_even ? c.template get<"dfs">() : c.template get<"dfs_old">();

			if (info({0u}).get() > 0u) {
				for (uint64_t k = 0u; k < Desc::Q; ++k) {
					auto dir = dfi::directions[dfi::opposite_index[k]];
					size_t acc_old_id = (j+dir[1]) * meta.at({0u}).get() + (i+dir[0]);

					auto& dfs_old = is_even ? cells[acc_old_id].template get<"dfs_old">() : cells[acc_old_id].template get<"dfs">();
					auto& info_old = cells[acc_old_id].template get<"info">();

					dfs_new({k}) = dfs_old({k});
				}
			}

		});
	}).wait();
}
}
}

template<typename T, typename Desc>
saw::error_or<void> kel_main(int argc, char** argv){
	using namespace kel;

	using dfi = lbm::df_info<T,Desc>;

	auto eo_lbm_dir = lbm::output_directory();
	if(eo_lbm_dir.is_error()){
		return std::move(eo_lbm_dir.get_error());
	}
	auto& lbm_dir = eo_lbm_dir.get_value();
	auto out_dir = lbm_dir / "poiseulle_channel_2d_gpu";
	std::filesystem::create_directories(out_dir);
	// 
	lbm::converter<lbm::sch::Float64> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};

	uint64_t x_d = 1024u;
	uint64_t y_d = 512u;

	saw::data<lbm::sch::FixedArray<lbm::sch::UInt64,Desc::D>> meta{{x_d,y_d}};

	acpp::sycl::queue sycl_q;
	SyclHostAlloc<lbm::sch::MacroStruct> sycl_host_alloc{sycl_q};
	// SyclDeviceAlloc<lbm::sch::CellStruct> sycl_dev_alloc{sycl_q};

	std::vector<saw::data<lbm::sch::MacroStruct>, SyclHostAlloc<lbm::sch::MacroStruct>> host_cells{x_d * y_d,sycl_host_alloc};

	saw::data<lbm::sch::CellStruct>* cells = acpp::sycl::malloc_device<saw::data<lbm::sch::CellStruct>>(x_d * y_d,sycl_q);
	saw::data<lbm::sch::MacroStruct>* macro_cells = acpp::sycl::malloc_device<saw::data<lbm::sch::MacroStruct>>(x_d * y_d,sycl_q);
	{
		auto eov = lbm::set_geometry<Desc>(cells,meta,sycl_q);
		if(eov.is_error()){
			return eov;
		}
	}


	uint64_t time_max = 1024u*128u;
	for(uint64_t i = 0u; i < time_max; ++i){
		lbm::step<Desc>(cells,macro_cells,meta,i,sycl_q);
		sycl_q.wait();
		if(i%4u == 0u){
			std::string vtk_f_name{"tmp/poiseulle_2d_gpu_"};
			vtk_f_name += std::to_string(i) + ".vtk";
			// write_vtk_file(vtk_f_name,host_cells);
			sycl_q.memcpy(&host_cells[0u], macro_cells, x_d * y_d * sizeof(saw::data<lbm::sch::MacroStruct>) ).wait();
			lbm::write_vtk_file<lbm::sch::MacroStruct,Desc::D>(vtk_f_name, &host_cells[0], meta);
		}
	}

	sycl_q.wait();
	acpp::sycl::free(cells, sycl_q);
	acpp::sycl::free(macro_cells, sycl_q);
	sycl_q.wait();

	return saw::make_void();
}

int main(int argc, char** argv){
	auto eov = kel_main<kel::lbm::sch::T,kel::lbm::sch::D2Q9>(argc, argv);
	if(eov.is_error()){
		auto& err = eov.get_error();
		std::cerr<<"[Error] "<<err.get_category();
		auto err_msg = err.get_message();
		if(err_msg.size() > 0u){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;
		return err.get_id();
	}
	return 0;
}
