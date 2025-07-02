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

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;

	auto meta = latt.meta();
	for(saw::data<sch::UInt64> i{0u}; i < meta.at(0u); ++i){
		for(saw::data<sch::UInt64> j{0u}; j < meta.at(1u); ++j ){
			auto& cell = latt({{i,j}});
			auto& info = cell.template get<"info">();


		}
	}

}

void set_initial_conditions(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;

	typename saw::native_data_type<sch::T>::type rho = 1.0;
	auto meta = latt.meta();

	// Collide
	for(saw::data<sch::UInt64> i{0u}; i < meta.at(0u); ++i){
		for(saw::data<sch::UInt64> j{0u}; j < meta.at(1u); ++j ){
			auto& cell = latt({{i,j}});
			auto& info = cell.template get<"info">();

		}
	}
	/*
	{
		std::array<typename saw::native_data_type<sch::T>::type, sch::D2Q9::D> vel = {0.0, 0.0};
		auto eq = equilibrium<sch::D2Q9>(rho, vel);

		apply_for_cells([&eq](auto& cell, std::size_t i, std::size_t j){
			(void) i;
			(void) j;
			auto& dfs = cell.template get<"dfs">();
			auto& dfs_old = cell.template get<"dfs_old">();
			auto info = cell.template get<"info">()(0u).get();
			for(uint64_t k = 0; k < sch::D2Q9::Q; ++k){
				dfs(k).set(eq[k]);
				dfs_old(k).set(eq[k]);
			}
		}, latt);
	}
	*/
}

int main(int argc, char** argv){
	using namespace kel::lbm;

	std::string_view cfg_file_name = "config.json";
	if(argc > 1){
		cfg_file_name = argv[1];
	}

	auto eo_conf = load_lbm_config<sch::Float64,sch::Descriptor<2u,9u>>(cfg_file_name);
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

	print_lbm_meta<sch::Float64,sch::Descriptor<2u,9u>>(conv, {conf.template get<"kinematic_viscosity">()});

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{1024, 128}};
	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> lattice{dim};

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{dim};

	for(uint64_t i = 0u; i < 256u; ++i){

		{
			std::string vtk_f_name{"tmp/poiseulle_2d_"};
			vtk_f_name += std::to_string(i) + ".vtk";
			write_vtk_file(vtk_f_name, macros);
		}

		lbm_step(lattice, even_step, step);
	}

	return 0;
}
