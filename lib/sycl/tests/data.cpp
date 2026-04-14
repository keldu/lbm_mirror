#include <forstio/test/suite.hpp>

#include "../c++/lbm.hpp"

namespace {

namespace sch {
using namespace kel::lbm::sch;
using TestObjSchema = Tuple<
	Member<FixedArray<UInt64,2u,2u>, "foo">,
	Member<Array<Float32>, "bar">,
	Member<
		Array<
			Struct<
				Member<FixedArray<Float32,2u>,"pos">
			>
		>,
		"baz"
	>
>;
}

SAW_TEST("Sycl Data Compilation"){
	acpp::sycl::queue q;
	saw::data<
		saw::schema::Struct<
			saw::schema::Member<
				kel::lbm::sch::Chunk<saw::schema::UInt8,1u,1u,1u>,
				"test"
			>
		>,
		kel::lbm::encode::Sycl<saw::encode::Native>
	> dat{q};

	auto& test_f = dat.template get<"test">();

	// test_f.at({}).set(1);
	// SAW_EXPECT(test_f.at({}).get() == 1, "Value check failed");
}

SAW_TEST("Sycl Data Compilation for Particle Similacrum"){
	acpp::sycl::queue q;

	saw::data<
		sch::TestObjSchema
	> a;
}
}
