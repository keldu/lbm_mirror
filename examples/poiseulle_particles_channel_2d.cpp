#include "../c++/lbm.hpp"
#include "../c++/collision.hpp"
#include "../c++/boundary.hpp"
#include "../c++/iterator.hpp"
#include "../c++/particle/geometry/circle.hpp"

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

template<typename Desc>
using CellParticleMask = Cell<UInt16, D2Q9, 1u, 0u, 0u>;

template<typename Desc>
using CellForceField = Cell<Float64, D2Q9, 0u, 1u, 0u>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">,
	Member<CellParticleMask<Desc>, "particle_mask">,
	Member<CellForceField<Desc>, "forcing">
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<FixedArray<T,D>, "velocity">,
	Member<T, "pressure">,
	Member<UInt16, "particle">
>;

template<typename T>
using GeometryStruct = Struct<
	Member<T, "info">
>;

using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;
}

namespace cmpt {
template<bool East>
struct PressureBoundaryRestrictedVelocityTo {};
}

/**
 * This is massively hacky and expects a lot of conditions
 * Either this or mirrored along the horizontal line works
 *
 * 0 - 2 - 2
 * 0 - 3 - 1
 * 0 - 3 - 1
 * .........
 * 0 - 3 - 1
 * 0 - 2 - 2
 *
 */
template<typename FP,typename Descriptor, bool East>
struct component<FP,Descriptor, cmpt::PressureBoundaryRestrictedVelocityTo<East>> {
private:
	saw::data<FP> pressure_setting_;
	saw::data<FP> rho_setting_;
public:
	component(const saw::data<FP>& pressure_setting__):
		pressure_setting_{pressure_setting__},
		rho_setting_{pressure_setting__ * df_info<FP,Descriptor>::inv_cs2}
	{}

	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, uint64_t time_step){
		using dfi = df_info<FP,Descriptor>;

		bool is_even = ((time_step % 2) == 0);
		auto& cell = field(index);

		auto& info = cell.template get<"info">();
		if(info({0u}).get() == 0u){
			return;
		}
		auto& dfs_old = (is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		/**
		 * Sum all known DFs
		 */
		saw::data<FP> sum_df{0};
		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Descriptor::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			auto& cell_n = field({{index.at({0u})+c_k[0u], index.at({1u})+c_k[1u]}});
			auto& info_n = cell_n.template get<"info">();
			auto info_n_val = info_n({0u});
			auto k_opp = dfi::opposite_index[k.get()];

			if(info_n_val.get() > 0u){
				sum_df += dfs_old({k_opp});
			}
		}
		/**
		 * Get the sum of the unknown dfs and precalculate the direction
		 */
		constexpr int known_dir = East ? 1 : -1;
		auto sum_unknown_dfs = (rho_setting_ - sum_df) * saw::data<FP>{known_dir};

		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Descriptor::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			auto& cell_n = field({{index.at({0u})+c_k[0u], index.at({1u})+c_k[1u]}});
			auto& info_n = cell_n.template get<"info">();
			auto info_n_val = info_n({0u});
			auto k_opp = dfi::opposite_index[k.get()];

			if(info_n_val.get() > 0u){
				sum_unknown_dfs += dfs_old({k}) * c_k[0u];
			}
		}

		auto vel_x = sum_unknown_dfs / rho_setting_;

		if constexpr (East) {
			dfs_old({2u}) = dfs_old({1u}) + saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({6u}) = dfs_old({5u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
			dfs_old({8u}) = dfs_old({7u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
		}else if constexpr (not East){
			dfs_old({1u}) = dfs_old({2u}) - saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({5u}) = dfs_old({6u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
			dfs_old({7u}) = dfs_old({8u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
		}
	}
};
}
}

void set_geometry(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;

	auto meta = latt.meta();

	/**
	 * Set ghost
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(0u);

	}, {{0u,0u}}, meta);

	/**
	 * Set wall
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(2u);

	}, {{0u,0u}}, meta, {{1u,1u}});

	/**
	 * Set fluid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(1u);

	}, {{0u,0u}}, meta, {{2u,2u}});

	/**
	 * Set inflow
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(3u);

	}, {{1u,0u}}, {{2u,meta.at({1u})}}, {{0u,2u}});

	/**
	 * Set outflow
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(4u);

	}, {{meta.at({0u})-2u,0u}}, {{meta.at({0u})-1u, meta.at({1u})}}, {{0u,2u}});

	/**
	 * Set channel wall
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		saw::data<sch::FixedArray<sch::UInt64,2u>> area{{meta.at({0u})-8u, meta.at({1u})-2u}};
		double safety_start = 0.1*area.at({0u}).get();
		double safety_width = 0.2*area.at({1u}).get();


		double ch_width = (area.at({1u}).get()-safety_width) * 0.5;
		double ch_length = (area.at({0u}).get()-safety_start) * 0.5;

		saw::data<sch::FixedArray<sch::UInt64,2u>> middle{{meta.at({0u})/2u, meta.at({1u})/2u}};

		// r^2 = (r-w/2)^2 + l^2
		double r_c = ch_length * ch_length;
		double r = r_c / (2.0 * ch_width);

		double top_pos = middle.at({1u}).get() + r + safety_width;
		double bot_pos = middle.at({1u}).get() - r - safety_width;
		double mid = middle.at({0u}).get();

		double r_2 = r*r;
		double dist_top = (top_pos - index.at({1u}).get());
		double dist_bot = (bot_pos - index.at({1u}).get());
		double dist_mid = (mid - index.at({0u}).get());

		double dist_top_sq = dist_top * dist_top + dist_mid * dist_mid;
		double dist_bot_sq = dist_bot * dist_bot + dist_mid * dist_mid;

		if(dist_top_sq < r_2 or dist_bot_sq < r_2){
			info({0u}).set(2u);
		}

	}, {{0u,0u}}, meta, {{4u,1u}});
}

void set_initial_conditions(saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt){
	using namespace kel::lbm;

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::D2Q9::D>> vel{{0.0,0.0}};
	auto eq = equilibrium<sch::T,sch::D2Q9>(rho, vel);

	auto meta = latt.meta();

	/**
	 * Set distribution
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& dfs = cell.template get<"dfs">();
		auto& dfs_old = cell.template get<"dfs_old">();

		for(saw::data<sch::UInt64> k = 0; k < saw::data<sch::UInt64>{sch::D2Q9::Q}; ++k){
			dfs(k) = eq.at(k);
			dfs_old(k) = eq.at(k);
		}

	}, {{0u,0u}}, meta);
}

void add_particles(kel::lbm::particle_system<sch::T,2u>& part_sys){
	saw::data<sch::Particle<sch::T,2u>> part;


	auto& p_mask = part.template get<"mask">();
	{
		particle_circle_geometry<sch::T> geo;
		p_mask = geo.template generate_mask<sch::T>(16u);
	}
	auto& rigid_body = part.template get<"rigid_body">();
	auto& p_size = part.template get<"size">();
	{
		auto& pos = rigid_body.template get<"position">();
		auto& old_pos = rigid_body.template get<"position">();

		pos = {{32u,64u}};
		old_pos = {{32u,64u}};

		p_size = {4.0};
	}

	{
		auto eov = particle_sys.add(part);
		if(eov.is_error()){
			exit(-1);
		}
	}
}

void couple_particles_to_lattice(
		kel::lbm::particle_system<sch::T,2u>& part_sys,
		saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
		saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>>& macros,
		saw::data<sch::T> time_step
){
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();
		auto& mask = cell.template get<"particle_mask">();

		for(saw::data<sch::UInt64> i{0u}; i < part_sys.size(); ++i){
			auto& part = part_sys.at(i);

			auto& p_rb = part.template get<"rigid_body">();
			auto& p_pos = p_rb.template get<"position">();
			auto& p_rot = p_rb.template get<"rotation">();

			auto& p_mask = part.template get<"mask">();
			auto& p_size = part.template get<"size">();

			// Rotated x-dir
			saw::data<sch::FixedArray<sch::T,2u>> x_dir{{
				p_size * saw::data<sch::T>{std::cos(p_rot.get())},
				p_size * saw::data<sch::T>{-std::sin(p_rot.get())}
			}};

			// Rotated y-dir
			saw::data<sch::FixedArray<sch::T,2u>> y_dir{{
				p_size * saw::data<sch::T>{std::sin(p_rot.get())},
				p_size * saw::data<sch::T>{std::cos(p_rot.get())}
			}};

			// Particle to Fluid Coupling
			// Spread force to close fluid cells
			iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
				(void)index;
			}, {{0u,0u}}, p_mask.dims());

			// Fluid to Particle Coupling
			iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
				saw::data<sch::FixedArray<sch::T,2u>> mask_shift{{
					index.at({0u}) * x_dir.at({0u}) + index.at({0u}) * y_dir.at({0u}),
					index.at({1u}) * x_dir.at({1u}) + index.at({1u}) * y_dir.at({1u})
				}};

				// Lagrange in Euler
				auto p_pos_lie = p_pos + mask_shift;
				// Cast down to get lower corner.
				// Before casting shift by 0.5 for closest pick
				saw::data<sch::FixedArray<sch::UInt64,2u>> p_cell_pos {{
					static_cast<uint64_t>(mask_shift.at({0u}).get()+0.5),
					static_cast<uint64_t>(mask_shift.at({1u}).get()+0.5)
				}};
				// Interpolate this from close U cells.
				// For now pick the closest U
				auto& closest_u = macros.at(p_cell_pos).template get<"velocity">();
				auto p_shift = closest_u;
				for(saw::data<sch::UInt64> i{0u}; i < 2u; ++i){
					p_shift.at(i) = p_shift.at(i) * time_step;
				}



				// Add forces to put away from walls
				/// 1. Check if neighbour is wall
				/// 2. Add force pushing away from wall

				// Add forces to push away from other particles

			}, {{0u,0u}}, p_mask.dims());
		}

	}, {{0u,0u}}, meta);
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
	uint64_t time_step
){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,sch::D2Q9>;

	bool even_step = ((time_step % 2u) == 0u);
  /**
	* 1. Relaxation parameter \tau
	*/
	component<sch::T, sch::D2Q9, cmpt::BGK> coll{0.5384};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<true>> inlet{1.01 * dfi::cs2};
	component<sch::T, sch::D2Q9, cmpt::PressureBoundaryRestrictedVelocityTo<false>> outlet{1.0 * dfi::cs2};

	auto meta = latt.meta();

	/**
	 * Collision
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		auto& dfs = cell.template get<"dfs">();
		auto& dfs_old = cell.template get<"dfs_old">();

		switch(info({0u}).get()){
			case 1u: {
				coll.apply(latt, index, time_step);
				break;
			}
			case 2u: {
				bb.apply(latt, index, time_step);
				break;
			}
			case 3u: {
				inlet.apply(latt, index, time_step);
				break;
			}
			case 4u: {
				outlet.apply(latt, index, time_step);
				break;
			}
			default:
				break;
		}

	}, {{0u,0u}}, meta);

	// Stream
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
		auto& info_new = cell.template get<"info">();

		if(info_new({0u}).get() > 0u){
			for(uint64_t k = 0u; k < sch::D2Q9::Q; ++k){
				auto dir = dfi::directions[dfi::opposite_index[k]];
				auto& cell_dir_old = latt({{index.at({0u})+dir[0],index.at({1u})+dir[1]}});

				auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();

				df_new({k}) = df_old({k});
			}
		}
	}, {{0u,0u}}, meta);
}

int main(int argc, char** argv){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,sch::D2Q9>;

	auto eo_lbm_dir = output_directory();
	if(eo_lbm_dir.is_error()){
		return -1;
	}
	auto& lbm_dir = eo_lbm_dir.get_value();
	auto out_dir = lbm_dir / "poiseulle_channel_2d";

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

	converter<sch::Float64> conv {
		{conf.template get<"delta_x">()},
		{conf.template get<"delta_t">()}
	};

	print_lbm_meta<sch::Float64,sch::Descriptor<2u,9u>>(conv, {conf.template get<"kinematic_viscosity">()});

	saw::data<sch::FixedArray<sch::UInt64,sch::D2Q9::D>> dim{{1024u, 128u}};
	saw::data<sch::CavityFieldD2Q9, saw::encode::Native> lattice{dim};
	auto meta = lattice.meta();

	/**
	 * Particle System
	 */
	particle_system<sch::T, 2u> particle_sys;
	add_particles(particle_sys);


	/**
	 * Setup geometry
	 */
	set_geometry(lattice);

	{

		saw::data<sch::Array<sch::GeometryStruct<sch::T>, sch::D2Q9::D>> geo{dim};

		iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
			auto& cell = lattice(index);
			auto& info = cell.template get<"info">();

			geo(index).template get<"info">().set(info({0u}).get());
		}, {{0u,0u}}, dim);

		write_vtk_file(out_dir / "geometry.vtk", geo);
	}

	/**
	 * Setup DFs
	 */
	set_initial_conditions(lattice);

	saw::data<sch::Array<sch::MacroStruct<sch::T,sch::D2Q9::D>,sch::D2Q9::D>> macros{dim};

	for(uint64_t i = 0u; i < 4096u*16u; ++i){
		bool even_step = ((i % 2u) == 0u);

		{
			// Stream
			iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
				auto& cell = lattice(index);
				auto& dfs = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();

				auto& rho = macros.at(index).template get<"pressure">();
				auto& vel = macros.at(index).template get<"velocity">();
				auto& part_mask = macros.at(index).template get<"particle">();
				compute_rho_u<sch::T,sch::D2Q9>(dfs,rho,vel);
				rho = rho * saw::data<sch::T>{dfi::cs2};
				part = cell.template get<"particle_mask">()({0u});
			}, {{0u,0u}}, meta);



			{
				std::string vtk_f_name{"macros_"};
				vtk_f_name += std::to_string(i) + ".vtk";
				write_vtk_file(out_dir / vtk_f_name, macros);
			}
		}

		couple_particles_to_lattice(particle_sys, lattice);
		lbm_step(lattice, i);
	}

	return 0;
}
