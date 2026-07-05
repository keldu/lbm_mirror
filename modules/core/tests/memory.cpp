#include <forstio/test/suite.hpp>

#include "../c++/memory.hpp"

namespace {
namespace sch {
using namespace saw::schema;

using TStruct = Struct<
	Member<Float32,"a">,
	Member<Int8, "b">,
	Member<UInt64, "c">
>;

using TChunk = kel::lbm::sch::Chunk<Float32,1u,2u,3u,4u>;
using TBChunk = kel::lbm::sch::Chunk<UInt32,1u,2u,3u,4u>;

using TChunkStruct = Struct<
	Member<TChunk, "a">,
	Member<TBChunk, "b">
>;
}

SAW_TEST("Memory Estimate"){
	using namespace kel::lbm;

	SAW_EXPECT((memory_estimate<sch::Float32>().get() == 4u), "Float32 isn't 4 bytes" );
	SAW_EXPECT((memory_estimate<sch::FixedArray<sch::Float32,5u,3u>>().get() == 60u), "FixedArray<Float32,5u,3u> isn't 60 bytes");
	SAW_EXPECT((memory_estimate<sch::FixedArray<sch::Float32,5u,3u>, sch::UInt8>().get() == 61u), "FixedArray<Float32,5u,3u> + UInt8 isn't 61 bytes");
}

SAW_TEST("Memory Estimate Struct"){
	using namespace kel::lbm;
	
	SAW_EXPECT((memory_estimate<sch::TStruct>().get() == 13u), "TStruct isn't 13 bytes" );
	// SAW_EXPECT((memory_estimate<sch::TStruct>().get() == 13u), "TStruct isn't 13 bytes" );
}

SAW_TEST("Memory Estimate Scalar"){
	using namespace kel::lbm;
	
	SAW_EXPECT((memory_estimate<sch::Scalar<sch::UInt8>>().get() == 1u), "Scalar of UInt8 isn't 1 bytes" );
	// SAW_EXPECT((memory_estimate<sch::TStruct>().get() == 13u), "TStruct isn't 13 bytes" );
}

SAW_TEST("Memory Estimate Chunk"){
	using namespace kel::lbm;
	
	SAW_EXPECT((memory_estimate<sch::TChunk>().get() == 480u), std::string{"TChunk isn't 480 bytes, but is "} + std::to_string(memory_estimate<sch::TChunk>().get()) );
}

SAW_TEST("Memory Estimate Struct of Chunk"){
	using namespace kel::lbm;
	
	SAW_EXPECT((memory_estimate<sch::TChunkStruct>().get() == 960u), std::string{"TChunkStruct isn't 960 bytes, but is "} + std::to_string(memory_estimate<sch::TChunk>().get()) );
}

}
