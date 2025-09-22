#include "../c++/lbm.hpp"
#include "../c++/collision.hpp"
#include "../c++/boundary.hpp"
#include "../c++/iterator.hpp"
#include "../c++/particle/geometry/circle.hpp"

#include <forstio/codec/data.hpp>
#include <forstio/codec/math.hpp>

#include <cmath>

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
// using T = MixedPrecision<Float64, Float32>;
using D2Q5 = Descriptor<2u,5u>;
using D2Q9 = Descriptor<2u,9u>;

template<typename Desc>
using DfCell = Cell<T, Desc, 0u, 0u, 1u>;

template<typename Desc>
using CellInfo = Cell<UInt8, D2Q9, 1u, 0u, 0u>;

template<typename Desc>
using CellParticleMask = Cell<UInt16, D2Q9, 1u, 0u, 0u>;

template<typename Desc>
using CellForceField = Cell<T, D2Q9, 0u, 1u, 0u>;

/**
 * Basic type for simulation
 */
template<typename Desc>
using CellStruct = Struct<
	Member<DfCell<Desc>, "dfs">,
	Member<DfCell<Desc>, "dfs_old">,
	Member<CellInfo<Desc>, "info">,
	// Member<CellParticleMask<Desc>, "particle_mask">,
	Member<CellForceField<Desc>, "force">
>;

template<typename T, uint64_t D>
using MacroStruct = Struct<
	Member<Vector<T,D>, "velocity">,
	Member<T, "pressure">,
	Member<UInt16, "particle">,
	Member<FixedArray<T,D>, "force">
	//Member<Vector<T,D>, "debug_acceleration">
>;

template<typename T>
using GeometryStruct = Struct<
	Member<T, "info">
>;

using CavityFieldD2Q9 = CellField<D2Q9, CellStruct<D2Q9>>;
}

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

		info({0u}).set(1u);

	}, {{0u,0u}}, meta, {{1u,1u}});

	/**
	 * Set fluid
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(2u);

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
			info({0u}).set(1u);
		}

	}, {{0u,0u}}, meta, {{4u,1u}});

	/**
	 * Set channel circular obstacle
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		saw::data<sch::FixedArray<sch::UInt64,2u>> area{{meta.at({0u})-4u, meta.at({1u})-4u}};
		double pos_x = 0.8 * area.at({0u}).get();
		double pos_y = 0.5 * area.at({1u}).get();

		double dist_x = pos_x - index.at({0u}).get();
		double dist_y = pos_y - index.at({1u}).get();

		double r = 16.0;
		double dist_sq = dist_x * dist_x + dist_y * dist_y;
		if(dist_sq < r*r){
			info({0u}).set(1u);
		}

	}, {{0u,0u}}, meta, {{2u,2u}});
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

void add_particles(kel::lbm::particle_system<kel::lbm::sch::T,2u>& part_sys){
	using namespace kel::lbm;
	saw::data<sch::Particle<sch::T,2u>> part;

	auto& p_mask = part.template get<"mask">();
	{
		particle_circle_geometry<sch::T> geo;
		p_mask = geo.template generate_mask<sch::T>(40u,0u);
	}
	auto& rigid_body = part.template get<"rigid_body">();
	auto& p_size = part.template get<"size">();
	auto& pos = rigid_body.template get<"position">();
	auto& old_pos = rigid_body.template get<"position_old">();
	{

		pos.at({{0u}}) = {32u};
		pos.at({{1u}}) = {64u};
		old_pos.at({{0u}}) = {32u};
		old_pos.at({{1u}}) = {64u};

		p_size = {1.0};
	}

	for(uint64_t i = 0; i < 1; ++i){
		for(uint64_t j = 0; j < 1; ++j){
			pos.at({{0u}}) = {static_cast<typename saw::native_data_type<sch::T>::type>(32u + j * 8u)};
			pos.at({{1u}}) = {static_cast<typename saw::native_data_type<sch::T>::type>(64u + i * 8u)};
			old_pos = pos;

			auto eov = part_sys.add_particle(part);
			if(eov.is_error()){
				exit(-1);
			}
		}
	}
}

void couple_particles_to_lattice(
		kel::lbm::particle_system<kel::lbm::sch::T,2u>& part_sys,
		saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
		saw::data<kel::lbm::sch::Array<kel::lbm::sch::MacroStruct<kel::lbm::sch::T,kel::lbm::sch::D2Q9::D>,kel::lbm::sch::D2Q9::D>>& macros,
		saw::data<kel::lbm::sch::T> time_step
){
	using namespace kel::lbm;
	(void) time_step;

	auto meta = latt.meta();
	using dfi = df_info<sch::T,sch::D2Q9>;

	for(saw::data<sch::UInt64> i{0u}; i < part_sys.size(); ++i){
		auto& part = part_sys.at(i);

		auto& p_rb = part.template get<"rigid_body">();
		auto& p_pos = p_rb.template get<"position">();
		auto& p_pos_old = p_rb.template get<"position_old">();
		auto& p_rot = p_rb.template get<"rotation">();

		auto& p_acc = p_rb.template get<"acceleration">();
		p_acc.at({{0u}}).set(0.0);
		p_acc.at({{1u}}).set(0.0);

		auto p_vel = p_pos - p_pos_old;

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

		auto& p_mask_grid = p_mask.template get<"grid">();
		// Get grid so we don't pull all the time
		auto p_mask_grid_dims = p_mask_grid.dims();

		saw::data<sch::Vector<sch::T,2u>> p_mask_grid_shift;
		p_mask_grid_shift.at({{0u}}) = (p_mask_grid_dims.at({0u}).template cast_to<sch::T>() - 1.0) / 2.0;
		p_mask_grid_shift.at({{1u}}) = (p_mask_grid_dims.at({1u}).template cast_to<sch::T>() - 1.0) / 2.0;

		// Particle to Fluid Coupling
		// Spread force to close fluid cells
		iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
			(void)index;
		}, {{0u,0u}}, p_mask_grid_dims);

		// Fluid to Particle Coupling
		// Prepare force sum
		// saw::data<sch::Vector<sch::T,2u>> forces;

		// Maybe in the future?
		// bool collision_happened = false;

		iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){

			if(p_mask_grid.at(index).get() == 0){
				return;
			}

			saw::data<sch::Vector<sch::T,2u>> index_shift;
			index_shift.at({{0u}}) = index.at({0u}).template cast_to<sch::T>() - p_mask_grid_shift.at({{0u}});
			index_shift.at({{1u}}) = index.at({0u}).template cast_to<sch::T>() - p_mask_grid_shift.at({{1u}});


			saw::data<sch::Vector<sch::T,2u>> mask_shift;
			mask_shift.at({{0u}}) = index_shift.at({{0u}}) * x_dir.at({0u}) + index_shift.at({{1u}}) * y_dir.at({0u});
			mask_shift.at({{1u}}) = index_shift.at({{0u}}) * x_dir.at({1u}) + index_shift.at({{1u}}) * y_dir.at({1u});

			auto p_pos_lie = p_pos + mask_shift;

			// Cast down to get lower corner.
			// Before casting shift by 0.5 for closest pick
			saw::data<sch::FixedArray<sch::UInt64,2u>> p_cell_pos;

			{
				p_cell_pos.at({{0u}}) = {static_cast<uint64_t>(p_pos_lie.at({{0u}}).get()+0.5)};
				p_cell_pos.at({{1u}}) = {static_cast<uint64_t>(p_pos_lie.at({{1u}}).get()+0.5)};
			}

			p_cell_pos.at({{0u}}).set(std::max(1ul, std::min(p_cell_pos.at({{0u}}).get(), meta.at({0u}).get()-2ul)));
			p_cell_pos.at({{1u}}).set(std::max(1ul, std::min(p_cell_pos.at({{1u}}).get(), meta.at({1u}).get()-2ul)));

			saw::data<sch::Vector<sch::UInt64,2u>> p_vec_cell_pos;
			p_vec_cell_pos.at({{0u}}) = p_cell_pos.at({0u});
			p_vec_cell_pos.at({{1u}}) = p_cell_pos.at({1u});

			// Interpolate this from close U cells.
			// For now pick the closest U
			auto& closest_u = macros.at(p_cell_pos).template get<"velocity">();
			// auto p_shift = closest_u;

			// p_pos - p_pos_old
			// auto p_vel_rel = closest_u - p_vel;
			saw::data<sch::Vector<sch::T, 2u>> p_vel_rel = closest_u - p_vel;

			// Add relative velocity to particle acceleration (Since our timestep is 1 we don't need to do anything else)
			p_acc = p_acc + p_vel_rel;

			/*
			for(saw::data<sch::UInt64> i{0u}; i.get() < 2u; ++i){
				p_acc.at({{i}}) = p_acc.at({{i}}) + p_vel_rel.at({{i}});
			}
			*/

			// Add forces to put away from walls
			/// 1. Check if neighbour is wall

			auto& cell = latt(p_cell_pos);
			auto& p_info = cell.template get<"info">()({0u});
			if(p_info.get() <= 1u){
				// Fake solid normal
				auto solid_normal = p_pos - p_vec_cell_pos.template cast_to<sch::T>();
				solid_normal = saw::math::normalize(solid_normal);
				auto v_n = saw::math::dot(solid_normal,p_vel);


				{
					saw::data<sch::Scalar<sch::T>> one;
					one.at({}) = {1.0};
					auto vn_plus_one = one + v_n;
					solid_normal.at({{0u}}) = solid_normal.at({{0u}}) + vn_plus_one.at({});
					solid_normal.at({{1u}}) = solid_normal.at({{1u}}) + vn_plus_one.at({});
				}
				auto force_normal = solid_normal;
				p_acc = p_acc + force_normal;
			}
			/*
			for(saw::data<sch::UInt64> k{0u}; k.get() < sch::D2Q9::Q; ++k){
				auto n_p_cell_pos = saw::data<sch::FixedArray<sch::UInt64,2u>> {{
					p_cell_pos.at({{0u}}) + dfi::directions[k.get()][0u],
					p_cell_pos.at({{1u}}) + dfi::directions[k.get()][1u]
				}};

				auto& n_cell = latt(n_p_cell_pos);
				auto& n_info = n_cell.template get<"info">()({0u});
				auto& n_macro_cell = macros.at(n_p_cell_pos);
				auto& n_macro_cell_particle = n_macro_cell.template get<"particle">();

				// If neighbour is wall, then add force pushing the particle away
				if(n_info.get() <= 1u or (n_macro_cell_particle.get() != (i.get()+1u) and n_macro_cell_particle.get() > 0u) ) {
					// add to p_acc

					saw::data<sch::T> dist_dot{
						(p_pos.at({{0u}})-n_p_cell_pos.at({{0u}}).template cast_to<sch::T>()) * dfi::directions[dfi::opposite_index[k.get()]][0u]+
						(p_pos.at({{1u}})-n_p_cell_pos.at({{1u}}).template cast_to<sch::T>()) * dfi::directions[dfi::opposite_index[k.get()]][1u]
					};

					if(dist_dot.get() > 0){
						// TODO add if particle is close
						//auto& n_debug_acc = n_macro_cell.template get<"debug_acceleration">();

						saw::data<sch::Vector<sch::T,sch::D2Q9::D>> push_force;
						{
							push_force.at({{0u}}) = saw::data<sch::Int32>{1000000 * dfi::directions[dfi::opposite_index[k.get()]][0u]}.template cast_to<sch::T>();
							push_force.at({{1u}}) = saw::data<sch::Int32>{1000000 * dfi::directions[dfi::opposite_index[k.get()]][1u]}.template cast_to<sch::T>();
						}

						//n_debug_acc = n_debug_acc + push_force;
						p_acc = p_acc + push_force;
					}
				}
			}
			*/
			/// 2. Add force pushing away from wall

			// Add forces to push away from other particles

		}, {{0u,0u}}, p_mask_grid.dims());


		//p_acc.at({{0u}}).set(p_acc.at({{0u}}).get() / (p_mask.template get<"grid">().dims().at({{0u}}).get() *p_mask.template get<"grid">().dims().at({{1u}}).get() ));
		// p_acc.at({{1u}}).set(p_acc.at({{1u}}).get() / (p_mask.template get<"grid">().dims().at({{0u}}).get() *p_mask.template get<"grid">().dims().at({{1u}}).get() ));
	}
}

void lbm_step(
	saw::data<kel::lbm::sch::CavityFieldD2Q9>& latt,
	saw::data<kel::lbm::sch::Array<kel::lbm::sch::MacroStruct<kel::lbm::sch::T,kel::lbm::sch::D2Q9::D>,kel::lbm::sch::D2Q9::D>>& macros,
	uint64_t time_step
){
	using namespace kel::lbm;
	using dfi = df_info<sch::T,sch::D2Q9>;

	(void) macros;

	bool even_step = ((time_step % 2u) == 0u);
  /**
	* 1. Relaxation parameter \tau
	*/
	component<sch::T, sch::D2Q9, cmpt::BGKGuo> coll{0.59};
	component<sch::T, sch::D2Q9, cmpt::BounceBack> bb;
	component<sch::T, sch::D2Q9, cmpt::ZouHeHorizontal<true>> inlet{1.1 * dfi::cs2 * 2.0 / 3.0};
	component<sch::T, sch::D2Q9, cmpt::ZouHeHorizontal<false>> outlet{1.0 * dfi::cs2 * 2.0 / 3.0};

	auto meta = latt.meta();

	/**
	 * Collision
	 */
	iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
		auto& cell = latt(index);
		auto& info = cell.template get<"info">();

		//auto& dfs = cell.template get<"dfs">();
		//auto& dfs_old = cell.template get<"dfs_old">();

		switch(info({0u}).get()){
			case 1u: {
				bb.apply(latt, index, time_step);
				break;
			}
			case 2u: {
				coll.apply(latt, index, time_step);
				break;
			}
			case 3u: {
				inlet.apply(latt, index, time_step);
				//coll.apply(latt, index, time_step);
				break;
			}
			case 4u: {
				outlet.apply(latt, index, time_step);
				// coll.apply(latt, index, time_step);
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
	auto out_dir = lbm_dir / "poiseulle_particles_channel_2d";

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

	uint64_t lbm_steps = 10000u;
	for(uint64_t i = 0u; i < lbm_steps; ++i){
		print_progress_bar(i,lbm_steps-1u);
		bool even_step = ((i % 2u) == 0u);

		{
			// Stream
			iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){
				auto& cell = lattice(index);
				auto& dfs = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();

				auto& macro_cell = macros.at(index);

				auto& rho = macro_cell.template get<"pressure">();
				auto& vel = macro_cell.template get<"velocity">();
				auto& force = macro_cell.template get<"force">();

				auto& part_mask = macro_cell.template get<"particle">();
				compute_rho_u<sch::T,sch::D2Q9>(dfs,rho,vel);
				rho = rho * saw::data<sch::T>{dfi::cs2};

				for(uint64_t d = 0u; d < sch::D2Q9::D; ++d){
					force.at({d}) = cell.template get<"force">()({d});
				}

				part_mask.set(0u);
			}, {{0u,0u}}, meta);

			for(saw::data<sch::UInt64> i{0u}; i < particle_sys.size(); ++i){
				auto& p = particle_sys.at({i});

				auto& p_rb = p.template get<"rigid_body">();
				auto& p_pos = p_rb.template get<"position">();

				auto& p_mask = p.template get<"mask">();
				auto& p_mask_grid = p_mask.template get<"grid">();
				auto p_mask_grid_dims = p_mask_grid.dims();

				saw::data<sch::Vector<sch::T,2u>> p_mask_grid_shift;
				p_mask_grid_shift.at({{0u}}) = (p_mask_grid_dims.at({0u}).template cast_to<sch::T>() - 1.0) / 2.0;
				p_mask_grid_shift.at({{1u}}) = (p_mask_grid_dims.at({1u}).template cast_to<sch::T>() - 1.0) / 2.0;


				// Iterate
				iterate_over([&](const saw::data<sch::FixedArray<sch::UInt64,2u>>& index){

					if(p_mask_grid.at(index).get() == 0){
						return;
					}

					saw::data<sch::Vector<sch::T,2u>> index_shift;
					index_shift.at({{0u}}) = index.at({0u}).template cast_to<sch::T>() - p_mask_grid_shift.at({{0u}});
					index_shift.at({{1u}}) = index.at({1u}).template cast_to<sch::T>() - p_mask_grid_shift.at({{1u}});

					saw::data<sch::Vector<sch::T,2u>> mask_shift;
					mask_shift.at({{0u}}) = index_shift.at({{0u}});
					mask_shift.at({{1u}}) = index_shift.at({{1u}});

					auto p_pos_lie = p_pos + mask_shift;

					// Cast down to get lower corner.
					// Before casting shift by 0.5 for closest pick
					saw::data<sch::FixedArray<sch::UInt64,2u>> p_cell_pos {{
						static_cast<uint64_t>(p_pos_lie.at({{0u}}).get()+0.5),
						static_cast<uint64_t>(p_pos_lie.at({{1u}}).get()+0.5)
					}};


					if(p_cell_pos.at({0u}) >= dim.at({0u}) ){
						p_cell_pos.at({0u}).set(dim.at({0u}).get() - 1u );
					}
					if(p_cell_pos.at({1u}) >= dim.at({1u}) ){
						p_cell_pos.at({1u}).set(dim.at({1u}).get() - 1u );
					}

					macros(p_cell_pos).template get<"particle">().set(i.get() + 1u);
				}, {{0u,0u}}, p_mask_grid_dims);
			}

			{
				std::string vtk_f_name{"macros_"};
				vtk_f_name += std::to_string(i) + ".vtk";
				write_vtk_file(out_dir / vtk_f_name, macros);
			}
		}

		couple_particles_to_lattice(particle_sys, lattice, macros, {1u});
		particle_sys.step({1u});
		lbm_step(lattice, macros, i);
	}

	return 0;
}
