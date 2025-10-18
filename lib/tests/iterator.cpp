#include <forstio/test/suite.hpp>

#include "../c++/iterator.hpp"

#include <iostream>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}

SAW_TEST("Iterate"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{3u,3u}};

	lbm::iterate_over([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end);
}

SAW_TEST("Iterate with Distance 1"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{4u,4u}};

	lbm::iterate_over([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end, {{1u,1u}});
}
}
