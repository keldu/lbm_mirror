#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/particle/geometry/circle.hpp"


namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
}

SAW_TEST("Particle Coupling"){
	using namespace kel;
	lbm::particle_system<sch::T,2,sch::Particle<sch::T,2>> system;


}
}
