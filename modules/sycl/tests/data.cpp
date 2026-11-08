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
	kel::lbm::data<
		kel::lbm::sch::Struct<
			kel::lbm::sch::Member<
				kel::lbm::sch::Chunk<kel::lbm::sch::UInt8,1u,1u,1u>,
				"test"
			>
		>,
		kel::lbm::encode::Sycl<kel::lbm::enc::Native>
	> dat{q};

	auto& test_f = dat.template get<"test">();

	// test_f.at({}).set(1);
	// SAW_EXPECT(test_f.at({}).get() == 1, "Value check failed");
}

SAW_TEST("Sycl Data Array of Struct"){
	acpp::sycl::queue q;

	kel::lbm::data<sch::Array<sch::Float64>, kel::lbm::encode::Sycl<kel::lbm::enc::Native>> a{{{2u}},q};
}

/*
SAW_TEST("Sycl Data Compilation for Particle Similacrum"){
	acpp::sycl::queue q;

	kel::lbm::data<
		sch::TestObjSchema
	> a;
}
*/
}
