#include "common.hpp"

namespace kel {
namespace lbm {


template<typename T, typename Desc>
saw::error_or<void> kmain(int argc, char** argv){
	using dfi = df_info<T,Desc>;

	(void) argc;
	(void) argv;

	return saw::make_void();
}

}
}

int main(int argc, char** argv){
	using namespace kel::lbm;
	auto eov = kmain<sch::Float32,sch::D2Q9>(argc,argv);
	if(eov.is_error()){
		auto& err = eov.get_error();

		std::cerr<<"[Error]: "<<err.get_category();
		auto err_msg = err.get_message();
		if(not err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;
		return err.code();
	}
	return 0;
}
