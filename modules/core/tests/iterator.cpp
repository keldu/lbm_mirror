#include <forstio/test/suite.hpp>

#include "../c++/iterator.hpp"

#include <iostream>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}

SAW_TEST("Old Iterate"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{3u,3u}};

	lbm::iterate_over([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end);
}

SAW_TEST("CT Tuple Iterator"){
	using namespace kel;

	saw::data<
		sch::Tuple<
			sch::Float32,
			sch::Int16,
			sch::Float64,
			sch::UInt32
		>
	> tup;

	lbm::ct_tuple_iterator<std::decay_t<decltype(tup)>::Schema>::apply([](auto& val) -> saw::error_or<void> {
		return saw::make_void();
	},tup);
}

SAW_TEST("Old Iterate with Distance 1"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{4u,4u}};

	lbm::iterate_over([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end, {{1u,1u}});
}

SAW_TEST("Iterate - 2D"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{3u,3u}};

	lbm::iterator<2u>::apply([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end);
}

SAW_TEST("Iterate with Distance 1 - 2D"){
	using namespace kel;

	saw::data<sch::FixedArray<sch::UInt64,2u>> start{{0u,0u}};
	saw::data<sch::FixedArray<sch::UInt64,2u>> end{{4u,4u}};

	lbm::iterator<2u>::apply([](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		std::cout<<"Index: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
	}, start, end, {{1u,1u}});
}
}
