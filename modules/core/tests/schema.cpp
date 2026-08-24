#include <forstio/test/suite.hpp>

#include "../c++/abstract/schema.hpp"
#include "../c++/abstract/stringify_schema.hpp"

namespace {
namespace ts {
using namespace kel::lbm::sch;

// using TPtr = Ptr<UInt16>;
using TVal = UInt16;
}
/*
SAW_TEST("Abstract/Schema/Ptr"){
	using namespace kel::lbm;

	// static_assert(std::is_same_v<typename schema_decay<ts::TPtr>::Schema, ts::TVal>, "schema decay failed");
}
*/

/*
SAW_TEST("Abstract/SchemaStringify/PrimitiveKind"){
	using namespace kel::lbm;

	using TSch = ts::UnsignedInteger;

	static constexpr auto str = schema_to_string<TSch>::value;
}
*/
}
