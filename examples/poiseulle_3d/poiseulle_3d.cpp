#include <kel/lbm/lbm.hpp>

namespace kel {
namespace lbm {
namespace sch {
using T = Float32;
using D3Q27 = Descriptor<3u,27u>;


}
saw::error_or<void> real_main(int argc, char** argv){


	return saw::make_void();
}

} // lbm
} // kel

/**
 * main, but I don't like the error handling
 */
int main(int argc, char** argv){
	auto eov = kel::lbm::real_main(argc, argv);
	if(eov.is_error()){
		auto& err = eov.get_error();
		auto err_msg = err.get_message();
		std::cerr<<"[Error]: "<<err.get_category();

		if(not err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;

		return err.get_id();
	}

	return 0;
}
