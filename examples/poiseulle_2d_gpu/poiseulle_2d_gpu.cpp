#include <kel/lbm/lbm.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>

#include <forstio/codec/data.hpp>

template<typename T>
using SyclHostAlloc = acpp::sycl::usm_allocator<saw::data<T>, acpp::sycl::usm::alloc::host>;

template<typename T>
using SyclDeviceAlloc = acpp::sycl::usm_allocator<saw::data<T>, acpp::sycl::usm::alloc::device>;

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

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">
>;

}

namespace cmpt {
template<bool East>
struct PressureBoundaryRestrictedVelocityTo {};
}

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

saw::error_or<void> set_geometry(
	saw::data<sch::CellStruct>* cells,
	const saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>>& meta,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;
	
	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::D2Q9::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,sch::D2Q9>(rho, vel);

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
				// Left output
				info.set({4u});
			}else {
				info.set({0u});
			}
			for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Desc::Q}; ++k){
				dfs(k) = eq.at(k);
				dfs_old(k) = eq.at(k);
			}
		});
	});

	sycl_q.wait();

	return saw::make_void();
}

void lbm_step(
	saw::data<sch::CellStruct>* cells,
	const saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>>& meta,
	uint64_t time_step,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,sch::D2Q9>;

	bool even_step = ((time_step % 2u) == 0u);
  /**
	* 1. Relaxation parameter \tau
	*/
  /*
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.5384};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<true>> inlet{1.01 * dfi::cs2};
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<false>> outlet{1.0 * dfi::cs2};
	*/
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

	// Create Dir TODO

	// 
	lbm::converter<lbm::sch::Float64> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};

	uint64_t x_d = 256u;
	uint64_t y_d = 64u;

	acpp::sycl::queue sycl_q;
	SyclHostAlloc<lbm::sch::CellStruct> sycl_host_alloc{sycl_q};
	// SyclDeviceAlloc<lbm::sch::CellStruct> sycl_dev_alloc{sycl_q};

	std::vector<saw::data<lbm::sch::CellStruct>, SyclHostAlloc<lbm::sch::CellStruct>> host_cells{x_d * y_d,sycl_host_alloc};

	saw::data<lbm::sch::CellStruct>* cells = acpp::sycl::malloc_device<saw::data<lbm::sch::CellStruct>>(x_d * y_d,sycl_q);

	{
		auto eov = lbm::set_geometry(cells,{{x_d,y_d}},sycl_q);
		if(eov.is_error()){
			return eov;
		}
	}

	std::string vtk_f_name{"tmp/poiseulle_2d_gpu_"};
	vtk_f_name += std::to_string(0u) + ".vtk";
	// write_vtk_file(vtk_f_name,host_cells);

	for(uint64_t i = 0u; i < 1024u*1204u; ++i){
		if(i%128u == 0u){

		}
		lbm_step(cells,meta,i,sycl_q);

	}

	sycl_q.wait();
	sycl_q.memcpy(&host_cells[0u], cells, x_d * y_d * sizeof(saw::data<lbm::sch::CellStruct>) );
	sycl_q.wait();
	acpp::sycl::free(cells, sycl_q);
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
