#include <kel/lbm/lbm.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>


#include <kel/lbm/particle/geometry/circle.hpp>
#include <iostream>

namespace kel{
namespace lbm {
namespace sch {
using namespace saw::schema;
}
}

saw::error_or<void> lbm_main(int argc, char** argv){
	(void) argc;
	(void) argv;

	using namespace lbm;
	using namespace acpp;

	saw::data<sch::Array<sch::Particle<sch::Float32,2u>>> particles{256u};

	return saw::make_void();
}
}

int main(int argc, char** argv){
	auto eov = kel::lbm_main(argc, argv);
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
