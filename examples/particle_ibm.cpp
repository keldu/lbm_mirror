#include "../c++/descriptor.hpp"
#include "../c++/equilibrium.hpp"

#include <forstio/codec/data.hpp>

#include <iostream>

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
	Member<CellInfo<Desc>, "info">,
	Member<UInt8, "geometry">,
	Member<UInt32, "particle">
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">
>;

using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;
}
namespace cmpt {
struct BounceBack{};
struct MovingWall {};
struct BGK {};
struct ConstRhoBGK {};
}

/**
 * A reason for why a component based design is really good can be seen in my LR solver example
 *
 * Add Expression Templates and you're golden.
 */
template<typename Kind, typename Desc>
class component;

/**
 * Full-Way BounceBack. I suspect that the moving wall requires half-way bounce back.
 */
template<typename Desc>
class component<cmpt::BounceBack,Desc> {
public:

	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		using dfi = df_info<sch::T,Desc>;

		// Technically use .copy()
		auto df_cpy = dfs;

		for(uint64_t i = 1u; i < Desc::Q; ++i){
			dfs({i}) = df_cpy({dfi::opposite_index.at(i)});
		}
	}
};

/**
 * Full-Way moving wall Bounce back, something is not right here.
 * Technically it should reflect properly.
 */
template<typename Desc>
class component<cmpt::MovingWall, Desc> {
public:
	std::array<typename saw::native_data_type<sch::T>::type, Desc::D> lid_vel;

public:
	void apply(
		saw::data<sch::DfCell<Desc>>& dfs
	){
		using dfi = df_info<sch::T,Desc>;

		// Technically use .copy()
		/*
		auto dfs_cpy = dfs;

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs({dfi::opposite_index.at(i)}) = dfs_cpy({i}) - 2.0 * dfi::weights[i] * 1.0 * ( lid_vel[0] * dfi::directions[i][0] + lid_vel[1] * dfi::directions[i][1]) * dfi::inv_cs2;
		}
		*/
	}
};

template<typename Desc>
class component<cmpt::BGK, Desc> {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
public:
	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		typename saw::native_data_type<sch::T>::type rho;
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
		compute_rho_u<sch::T,Desc>(dfs,rho,vel);
		auto eq = equilibrium<Desc>(rho,vel);

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs({i}).set(dfs({i}).get() + (1.0 / relaxation_) * (eq[i] - dfs({i}).get()));
		}
	}
};

template<typename Desc>
class component<cmpt::ConstRhoBGK, Desc> {
public:
	typename saw::native_data_type<sch::T>::type relaxation_;
	typename saw::native_data_type<sch::T>::type rho_;
public:
	void apply(saw::data<sch::DfCell<Desc>>& dfs){
		std::array<typename saw::native_data_type<sch::T>::type, Desc::D> vel;
		compute_const_rho_u<Desc>(dfs,rho_,vel);
		auto eq = equilibrium<Desc>(rho_,vel);

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs({i}).set(dfs({i}).get() + (1.0 / relaxation_) * (eq[i] - dfs({i}).get()));
		}
	}
};

}
}

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;
	/*
	apply_for_cells([](auto& cell, std::size_t i, std::size_t j){
		uint8_t val = 0;
		if(j == 1){
			val = 2u;
		}
		if(i == 1 || (i+2) == dim_x || (j+2) == dim_y){
			val = 3u;
		}
		if(i > 1 && (i+2) < dim_x && j > 1 && (j+2) < dim_y){
			val = 1u;
		}
		if(i == 0 || j == 0 || (i+1) == dim_x || (j+1) == dim_y){
			val = 0u;
		}
		cell.template get<"info">()(0u).set(val);
	}, latt);
	*/
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
	bool even_step
){
	using namespace kel::lbm;
  using dfi = df_info<sch::T,sch::D2Q9>;

	component<cmpt::BGK,sch::D2Q9> coll;
	coll.relaxation_ = 0.5384;
	component<cmpt::ConstRhoBGK,sch::D2Q9> rho_coll;
	rho_coll.relaxation_ = 0.5384;
	rho_coll.rho_ = 1.0;

	component<cmpt::BounceBack,sch::D2Q9> bb;
	component<cmpt::MovingWall,sch::D2Q9> bb_lid;
	bb_lid.lid_vel = {0.1,0.0};

	// Collide
	apply_for_cells([&](auto& cell, std::size_t i, std::size_t j){
		auto& df = even_step ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& info = cell.template get<"info">();

		auto info_val = info({0u}).get();
		switch(info_val){
		case 1u:
			coll.apply(df);
			break;
		case 2u:
			// bb.apply(df);
			bb_lid.apply(df);
			break;
		case 3u:
			bb.apply(df);
			break;
		}
	}, latt);

	// Stream
	for(uint64_t i = 1; (i+1) < latt.template get_dim_size<0>().get(); ++i){
		for(uint64_t j = 1; (j+1) < latt.template get_dim_size<1>().get(); ++j){
			auto& cell = latt({{i,j}});
			auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
			auto& info_new = cell.template get<"info">();

			if(info_new({0u}).get() > 0u && info_new({0u}).get() != 2u){
				for(uint64_t k = 0u; k < sch::D2Q9::Q; ++k){
					auto dir = dfi::directions[dfi::opposite_index[k]];
					auto& cell_dir_old = latt({{i+dir[0],j+dir[1]}});

					auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();
					auto& info_old = cell_dir_old.template get<"info">();

					if( info_old({0}).get() == 2u ){
						auto& df_old_loc = even_step ? latt({{i,j}}).template get<"dfs_old">() : latt({{i,j}}).template get<"dfs">();
						df_new({k}) = df_old_loc({dfi::opposite_index.at(k)}) - 2.0 * dfi::inv_cs2 * dfi::weights.at(k) * 1.0 * ( bb_lid.lid_vel[0] * dir[0] + bb_lid.lid_vel[1] * dir[1]);
						// dfs({dfi::opposite_index.at(i)}) = dfs_cpy({i}) - 2.0 * dfi::weights[i] * 1.0 * ( lid_vel[0] * dfi::directions[i][0] + lid_vel[1] * dfi::directions[i][1]) * dfi::inv_cs2;
					} else {
						df_new({k}) = df_old({k});
					}
				}
			}
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
	lbm::particle_system<sch::T,2,sch::Particle<sch::T,2>> system;

	/**
	 * Set meta information describing what this cell is
	 */
	set_geometry(lattice);


	uint64_t lattice_steps{128u};
	uint64_t print_every{64u};
	bool even_step{true};

	uint64_t file_num{0u};

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{dim};

	for(uint64_t step{0u}; step < lattice_steps; ++step){

		lbm_step(lattice, even_step);

		even_step = not even_step;

	}

	return 0;
}
