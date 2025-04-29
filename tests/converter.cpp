#include <forstio/test/suite.hpp>

#include "../c++/converter.hpp"


namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}

SAW_TEST("Si Meter to Lbm Meter"){
	using namespace kel;

	lbm::converter<sch::T> converter{
		{0.1},
		{0.1}
	};

	saw::data<sch::SiMeter<sch::T>> si_m{1.0};

	auto lbm_m = converter.meter_si_to_lbm(si_m);

	SAW_EXPECT(lbm_m.handle().get() == 10.0, "Correct si to lbm conversion");
}
}
