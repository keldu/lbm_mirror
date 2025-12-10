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

void set_geometry(
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs,
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs_old,
	std::vector<saw::data<sch::UInt8>, SyclDeviceAlloc<sch::UInt8>>& info,
	const saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>>& meta,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;

}

void set_initial_conditions(
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs,
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs_old,
	std::vector<saw::data<sch::UInt8>, SyclDeviceAlloc<sch::UInt8>>& info,
	const saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>>& meta,
	acpp::sycl::queue& sycl_q
){
	using namespace kel::lbm;

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::D2Q9::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,sch::D2Q9>(rho, vel);

}

void lbm_step(
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs,
	std::vector<saw::data<sch::Float64>, SyclDeviceAlloc<sch::Float64>>& dfs_old,
	std::vector<saw::data<sch::UInt8>, SyclDeviceAlloc<sch::UInt8>>& info,
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
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.5384};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<true>> inlet{1.01 * dfi::cs2};
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<false>> outlet{1.0 * dfi::cs2};

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
	lbm::converter<sch::Float64> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};

	uint64_t x_d = 1024u;
	uint64_t y_d = 128u;

	acpp::sycl::queue sycl_q;
	SyclHostAlloc<sch::CellStruct> sycl_host_alloc{sycl_q};
	SyclDeviceAlloc<sch::CellStruct> sycl_dev_alloc{sycl_q};

	std::vector<saw::data<sch::CellStruct>, SyclHostAlloc<sch::CellStruct>> cells{x_d * y_d,sycl_alloc};
	std::vector<saw::data<sch::CellStruct>, SyclDeviceAlloc<sch::CellStruct>> cells{x_d * y_d,sycl_alloc};

	{
		auto eov = set_geometry(cells);
		if(eov.is_error()){
			return eov;
		}
	}


	return saw::make_void();
}

int main(int argc, char** argv){
	auto eov = kel_main(argc, argv);
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
