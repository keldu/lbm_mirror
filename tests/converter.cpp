#include <forstio/test/suite.hpp>

#include "../c++/converter.hpp"


namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}

SAW_TEST("Meter"){
	using namespace kel;

	lbm::converter<sch::T> converter;
}
}
