#include "../c++/lbm.hpp"

#include <iostream>

int main(int argc, char** argv){
	using namespace kel::lbm;

	auto eo_conf = load_lbm_config<sch::Float64,sch::Descriptor<2,9>>("config.json");
	if(eo_conf.is_error()){
		auto& err = eo_conf.get_error();
		std::cerr<<"[Error]: "<<err.get_category();
		auto err_msg = err.get_message();
		if(!err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;

		return err.get_id();
	}

	auto& conf = eo_conf.get_value();
	
	converter<sch::Float64> conv{
		{0.1},
		{0.1}
	};

	print_lbm_meta<sch::Float64,sch::Descriptor<2,9>>(conv, {conf.template get<"kinematic_viscosity">()});

	return 0;
}
