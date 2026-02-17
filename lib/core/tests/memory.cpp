#include <forstio/test/suite.hpp>

#include "../c++/memory.hpp"

namespace {
namespace sch {
using namespace saw::schema;
}

SAW_TEST("Memory Estimate"){
	using namespace kel::lbm;

	SAW_EXPECT((memory_estimate<sch::Float32>().get() == 4u), std::string{"Float32 isn't 4 bytes, but "} + std::to_string(memory_estimate<sch::Float32>().get()) );
	SAW_EXPECT((memory_estimate<sch::FixedArray<sch::Float32,5u,3u>>().get() == 60u), "FixedArray<Float32,5u,3u> isn't 60 bytes");
	SAW_EXPECT((memory_estimate<sch::FixedArray<sch::Float32,5u,3u>, sch::UInt8>().get() == 61u), "FixedArray<Float32,5u,3u> + UInt8 isn't 61 bytes");
}

}
