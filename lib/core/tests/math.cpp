#include <forstio/test/suite.hpp>

#include "../c++/math/n_linear.hpp"

namespace {
namespace sch {
using namespace saw::schema;
}

SAW_TEST("Math 2-Linear"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::Vector<sch::Float64,2u>,32u,32u>> field;

	saw::data<sch::Vector<sch::Float64,2u>> pos;

	SAW_EXPECT(true, "Default true check");
}

}
