#include <kel/lbm/lbm.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<typename T, typename Desc>
using CellStruct = Struct<
	Member<FixedArray<T,Desc::Q>, "dfs">,
	Member<FixedArray<T,Desc::Q>, "dfs_old">,
	Member<UInt8, "info">,
	Member<Vector<T,Desc::D>, "velocity">,
	Member<Vector<T,Desc::D>, "force">
>;
}

template<typename T, typename Desc>
saw::error_or<void> step(){

	return saw::make_void();
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
	auto out_dir = lbm_dir / "poiseulle_particles_2d_gpu";

	lbm::converter<lbm::sch::Float64> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};

	uint64_t x_d = 256u;
	uint64_t y_d = 64u;
	saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{x_d,y_d}};

	acpp::sycl::queue sycl_q;

	sycl_q.wait();

	return saw::make_void();
}

using FloatT = kel::sch::Float32;

int main(int argc, char** argv){
	auto eov = kel_main<FloatT,kel::lbm::sch::D2Q9>(argc, argv);
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
}
