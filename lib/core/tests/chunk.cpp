#include <forstio/test/suite.hpp>

#include "../c++/chunk.hpp"

namespace {
namespace sch {
using namespace saw::schema;
}

SAW_TEST("Chunk Ghost size 0"){
	using namespace kel;

	using TestChunk = lbm::sch::Chunk<sch::UInt32, 0u, 2u, 3u, 4u>;
	using TestArray = sch::FixedArray<sch::UInt32, 2u, 3u, 4u>;

	SAW_EXPECT((std::is_same_v<TestChunk::InnerSchema,TestArray>), "Types are not identical");
}

SAW_TEST("Chunk Ghost size 1"){
	using namespace kel;

	using TestChunk = lbm::sch::Chunk<sch::UInt32, 1u, 2u, 3u, 4u>;
	using TestArray = sch::FixedArray<sch::UInt32, 4u, 5u, 6u>;

	SAW_EXPECT((std::is_same_v<TestChunk::InnerSchema,TestArray>), "Types are not identical");
}

SAW_TEST("Chunk Ghost size 3"){
	using namespace kel;

	using TestChunk = lbm::sch::Chunk<sch::UInt32, 3u, 2u, 3u, 4u,10u,6u>;
	using TestArray = sch::FixedArray<sch::UInt32, 8u, 9u, 10u,16u,12u>;

	SAW_EXPECT((std::is_same_v<TestChunk::InnerSchema,TestArray>), "Types are not identical");
}

}
