#include <forstio/test/suite.hpp>

#include <iostream>
#include "../c++/write_vtk.hpp"


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

}

SAW_TEST("VTK Write test example"){
	using namespace kel;

	// using Type = typename parameter_pack_type<i,T...>::type;
}
}
