#include <forstio/codec/args.hpp>

namespace kel {
namespace sch {
}

kel::lbm::error_or<void> kmain(int argc, char** argv){
	kel::lbm::data<sch::Args> args;
	// Decode the args
	{
		kel::lbm::data<sch::Args,encode::Args> arge;
		auto eov = saw::parse_args<sch::Args>(arge,args);
		if(eov.is_error()){
			return eov;
		}
	}
	return kel::lbm::make_void();
}

}

int main(int argc, char** argv){
	auto eov = kel::kmain(argc,argv);
	if(eov.is_error()){
		auto& err = eov.get_error();
	}
	return 0;
}
