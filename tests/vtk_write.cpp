#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/write_vtk.hpp"

#include <sstream>

namespace {
namespace sch {
using namespace kel::lbm::sch;

using T = Float64;
using D2Q5 = Descriptor<2,5>;
using D2Q9 = Descriptor<2,9>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0, 0, 1>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q9, 1, 0, 0>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">
>;

}

SAW_TEST("VTK Write test example"){
	using namespace kel;

	// write_vtk();

	std::stringstream sstream;

	saw::data<sch::Array<sch::MacroStruct<sch::T,2>, 2>> cells{{{2u,2u}}};

	auto& cell_0 = cells.at({{{0,0}}});
	cell_0.template get<"velocity">()= {{0.5,-0.1}};
	cell_0.template get<"pressure">().set({1.1});

	auto eov = lbm::impl::lbm_vtk_writer<sch::Array<sch::MacroStruct<sch::T,2>, 2>>::apply(sstream, cells);
	SAW_EXPECT(eov.is_value(), "vtk writer failed to write");

	std::cout<<sstream.str()<<std::endl;

	// using Type = typename parameter_pack_type<i,T...>::type;
}
}
