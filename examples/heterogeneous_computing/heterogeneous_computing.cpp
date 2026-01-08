#include <forstio/error.hpp>
#include <iostream>


namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
using KelConfig = Struct<
	Member<String,"resolution">
>;
}
}

saw::error_or<void> lbm_main(int argc, char** argv){
	return saw::make_void();
}
}

int main(int argc, char** argv){
	auto eov = kel::lbm_main(argc, argv);
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
