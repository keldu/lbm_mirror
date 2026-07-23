#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

#include "particle/particle.hpp"

#include <iostream>

namespace kel {
namespace lbm {
namespace cmpt {
struct HlbmReset {}; 
struct Hlbm {};
struct HlbmOneParticle {};
struct HlbmParticle {};
struct HlbmOneParticleMomentumExchange {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::HlbmReset, Encode> final {
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		auto& porosity_f = macros.template get<"porosity">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto& por = porosity_f.at(index);
		por.at({}) = 1.0;

		auto& pnf = particle_N_f.at(index);
		for(uint64_t i{0u}; i < Descriptor::D; ++i){
			pnf.at({{i}}) = 0.0;
		}

		auto& pnd = particle_D_f.at(index);
		pnd.at({}) = 0.0;
	}
};

/**
 * HLBM collision operator for LBM
 */
template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::Hlbm, Encode> final {
private:
	typename saw::native_data_type<T>::type relaxation_;
	saw::data<T> frequency_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index, saw::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto& porosity_f = macros.template get<"porosity">();
		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();

		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Desc::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Desc>(dfs_old_f.at(index), rho, vel);

		auto& porosity = porosity_f.at(index);
		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		auto flip_porosity = one - porosity;

		auto& N = particle_N_f.at(index);
		auto& D = particle_D_f.at(index);

		// Convex combination of velocities
		vel = vel * porosity + [&]() -> saw::data<sch::Vector<T,Desc::D>> {
			return (D.at({}).get() > 0.0) ? ( N * flip_porosity / D ): N;
		}();
		// Equilibrium
		auto eq = equilibrium<T,Desc>(rho,vel);

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}

	}
};

template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmOneParticle, Encode> final {
private:
/*
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema, uint64_t i>
	void apply_i(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& part_groups, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step) const {
		// if constexpr ( i < )
	}
*/
public:
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& part_group, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step, saw::data<sch::UInt64> sub_steps) const {
		/// Figure out how to access the particle list
		// auto& p = particles.at(i);

		/// Iterate over the grid bounds
		// auto& grid = p.template get<"grid">();
		using dfi = df_info<T,Desc>;
		bool is_even = ((time_step.get() % 2) == 0);
		
		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& part_spheroid_group = part_group;
		auto& mvel_f = macros.template get<"velocity">();
		auto& mpor_f = macros.template get<"porosity">();
		
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();
		{
			auto parts = part_spheroid_group.template get<"particles">();
			auto parts_size = parts.meta().at({0u});

			auto& p_coll = part_spheroid_group.template get<"collision">().at({});
			auto& p_rad = p_coll.template get<"radius">();

			auto& pi = parts.at(index);
			auto& pirb = pi.template get<"rigid_body">();
			saw::data<sch::Vector<T,Desc::D>>& pirb_pos = pirb.template get<"position">();
			auto& pirb_pos_old = pirb.template get<"position_old">();
				
			// TODO !!!! Divide by actual time step - for now it's ok
			saw::data<sch::Scalar<T>> ts;
			ts.at({}) = one.at({}) / sub_steps.template cast_to<T>();

			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> start;
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> stop;

			auto eo_aabb = particle_aabb<typename ParticleSchema::ValueType>::calculate(part_spheroid_group,{{0u}},mvel_f.meta());
			if(eo_aabb.is_error()){
				return;
			}
			auto& aabb = eo_aabb.get_value();

			/// Ok, I iterate over the space which covers our particle? So lower bounds to upper bounds
			start = aabb.template get<"a">();
			stop = aabb.template get<"b">();

			saw::data<sch::Vector<T,Desc::D>> force_p;
			for(uint64_t i{0u}; i < Desc::D; ++i){
				force_p.at({{i}}) = 0.0;
			}

			iterator<Desc::D>::apply([&](const auto& index_f) -> void{
				// ask for the d_k value here.
				// For every value im iterating over I need sth
				// std::cout<<"Pos: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
			
				auto& dfs = dfs_old_f.at(index_f);
				saw::data<sch::Vector<T,Desc::D>> rel_dist = saw::math::vectorize_data(index_f).template cast_to<T>() - pirb_pos;
				saw::data<sch::Scalar<T>> eps;
				eps.at({}) = 1.5f;
				
				auto& mpor = mpor_f.at(index_f);
				mpor = particle_porosity<T,Desc::D,1u,por::ParticleSpheroid<T>>::calculate(rel_dist,p_rad,eps);

				if(mpor.at({}).get() >= 1.0f){
					return;
				}

				saw::data<sch::Vector<T,Desc::D>> momentum;
				for(uint64_t i{0u}; i < Desc::D; ++i){
					momentum.at({{i}}) = 0.0;
				}

				for(uint64_t i{0u}; i < Desc::Q; ++i){
					saw::data<sch::Vector<T,Desc::D>> e_i;
					saw::data<sch::FixedArray<sch::UInt64,Desc::D>> n_ind_i;
					for(uint64_t k{0u}; k < Desc::D; ++k){
						e_i.at({{k}}) = (dfi::directions[i])[k];
						n_ind_i.at({k}) = index_f.at({k}) + (dfi::directions[i])[k];
					}

					uint64_t i_opp = dfi::opposite_index[i];

					saw::data<T> dfs_added = dfs.at({i}) - dfs_old_f.at(n_ind_i).at({i_opp});
					saw::data<sch::Scalar<T>> dfs_added_v;
					dfs_added_v.at({}) = dfs_added;
					auto ei_dfs = e_i * dfs_added_v;

					momentum = momentum + ei_dfs;
				}

				force_p = force_p + momentum;
			},start,stop);
			
			auto& pirb_acc = pirb.template get<"acceleration">();
			pirb_acc = force_p / part_spheroid_group.template get<"total_mass">().at({});

			for(saw::data<sch::UInt64> i{0u}; i < sub_steps; ++i){
				verlet_step_lambda<T,Desc::D>(pi,ts);
			}
			auto vel_p = (pirb_pos-pirb_pos_old) / ts;

			iterator<Desc::D>::apply([&](const auto& index_f){
				auto& N = particle_N_f.at(index_f);
				auto& D = particle_D_f.at(index_f);
				auto& mpor = mpor_f.at(index_f);
				auto flip_porosity = one - mpor;

				D = D + flip_porosity;
				N = N + vel_p * flip_porosity;
			},start,stop);
			// Check
		}
	}
};

template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmOneParticleMomentumExchange, Encode> final {
public:
	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index, saw::data<sch::UInt64> time_step) const {
		//
		using dfi = df_info<T,Desc>;
		bool is_even = ((time_step.get() % 2) == 0);
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs = dfs_old_f.at(index);

		saw::data<sch::Vector<T,Desc::D>> momentum;
		for(uint64_t i = 0u; i < Desc::Q; ++i){
			
			saw::data<sch::Vector<T,Desc::D>> e_i;
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> n_ind_i;
			for(uint64_t k{0u}; k < Desc::D; ++k){
				e_i.at({{k}}) = dfi::directions[i][k];
				n_ind_i.at({k}) = (dfi::directions[i])[k];
			}

			uint64_t i_opp = dfi::opposite_index[i];

			saw::data<T> dfs_added = dfs.at({i}) - dfs_old_f.at(n_ind_i).at({i_opp});
			saw::data<sch::Scalar<T>> dfs_added_v;
			dfs_added_v.at({}) = dfs_added;
			auto ei_dfs = e_i * dfs_added_v;

			momentum = momentum + ei_dfs;
		}
		auto& force_f = macros.template get<"force">();
		
		// Set Force
		force_f.at(index) = momentum * macros.template get<"porosity">().at(index);
	}
};
}
}
