#include "../c++/lbm.hpp"
#include "../c++/collision.hpp"
#include "../c++/boundary.hpp"

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

/**
 * Basic distribution function
 * Base type
 * D
 * Q
 * Scalar factor
 * D factor
 * Q factor
 */
using T = Float32;
using D2Q5 = Descriptor<2u,5u>;
using D2Q9 = Descriptor<2u,9u>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0u, 0u, 1u>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q9, 1u, 0u, 0u>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">
>;


using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;
}
}
}

int main(int argc, char** argv){
	using namespace kel::lbm;

	std::string_view cfg_file_name = "config.json";
	if(argc > 1){
		cfg_file_name = argv[1];
	}

	auto eo_conf = load_lbm_config<sch::Float64,sch::Descriptor<2,9>>(cfg_file_name);
	if(eo_conf.is_error()){
		auto& err = eo_conf.get_error();
		std::cerr<<"[Error]: "<<err.get_category();
		auto err_msg = err.get_message();
		if(!err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;

		return err.get_id();
	}

	auto& conf = eo_conf.get_value();

	converter<sch::Float64> conv{
		{conf.template get<"delta_x">()},
		{conf.template get<"delta_t">()}
	};

	print_lbm_meta<sch::Float64,sch::Descriptor<2,9>>(conv, {conf.template get<"kinematic_viscosity">()});

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{128, 128}};

	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> lattice{dim};

	return 0;
}
