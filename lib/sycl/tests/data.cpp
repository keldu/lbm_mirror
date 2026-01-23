#include <forstio/test/suite.hpp>

#include "../c++/lbm.hpp"

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
