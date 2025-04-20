#include "../c++/lbm.hpp"

int main(int argc, char** argv){
	using namespace kel::lbm;
	
	converter<sch::Float64> conv{
		{0.1},
		{0.1}
	};

	print_lbm_meta<sch::Float64,sch::Descriptor<2,9>>(conv, {1e-5});

	return 0;
}
