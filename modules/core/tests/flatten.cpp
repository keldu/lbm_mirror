#include <forstio/test/suite.hpp>

#include "../c++/flatten.hpp"

namespace {
namespace sch {
using namespace saw::schema;
}

SAW_TEST("Flatten Index Stride"){
	using namespace kel;

	constexpr saw::data<sch::UInt64> zero = lbm::flatten_index<sch::UInt64,3u>::stride<0u>({{2u,4u,3u}});
	constexpr saw::data<sch::UInt64> one  = lbm::flatten_index<sch::UInt64,3u>::stride<1u>({{2u,4u,3u}});
	constexpr saw::data<sch::UInt64> two  = lbm::flatten_index<sch::UInt64,3u>::stride<2u>({{2u,4u,3u}});

	SAW_EXPECT(zero.get() == 1u, "Zero is correct");
	SAW_EXPECT(one.get() == 2u, "Zero is correct");
	SAW_EXPECT(two.get() == 8u, "Zero is correct");
}

}
