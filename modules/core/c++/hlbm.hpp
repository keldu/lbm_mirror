#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

#include "particle/particle.hpp"

#include <iostream>

namespace kel {
namespace lbm {
namespace cmpt {
struct HlbmInit {}; 
struct Hlbm {};
struct HlbmParticle {};
struct HlbmOneParticleMomentumExchange {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::HlbmInit, Encode> final {
private:
	typename saw::native_data_type<T>::type relaxation_;
	saw::data<T> frequency_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		auto& porosity_f = macros.template get<"porosity">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto& por = porosity_f.at(index);
		por = {};

		auto& pnf = particle_N_f.at(index);
		pnf = {};

		auto& pnd = particle_D_f.at(index);
		pnd = {};
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
			return (D.at({}).get() > 0.0 ? N * flip_porosity / D : N);
		}();
		// Equilibrium
		auto eq = equilibrium<T,Desc>(rho,vel);

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}

		D.at({}) = 0.0;
		N = {};
	}
};

template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmParticle, Encode> final {
private:
/*
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema, uint64_t i>
	void apply_i(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& part_groups, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step) const {
		// if constexpr ( i < )
	}
*/
public:
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& part_group, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step) const {
		/// Figure out how to access the particle list
		// auto& p = particles.at(i);

		/// Iterate over the grid bounds
		// auto& grid = p.template get<"grid">();

		auto& part_spheroid_group = part_group;
		auto& mvel = macros.template get<"velocity">();
		{
			auto& parts = part_spheroid_group.template get<"particles">();
			auto parts_size = parts.meta().at({0u});

			auto& pi = parts.at(index);
			auto& pirb = pi.template get<"rigid_body">();
			auto& pirb_pos = pirb.template get<"position">();

			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> start;
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> stop;

			auto eo_aabb = particle_aabb<typename ParticleSchema::ValueType>::calculate(part_spheroid_group,index,mvel.meta());
			if(eo_aabb.is_error()){
				std::cerr<<"Prepping error"<<std::endl;
				std::cerr<<eo_aabb.get_error().get_category()<<std::endl;
				return;
			}
			auto& aabb = eo_aabb.get_value();

			/// Ok, I iterate over the space which covers our particle? So lower bounds to upper bounds
			start = aabb.template get<"a">();
			stop = aabb.template get<"b">();

			iterator<Desc::D>::apply([&](const auto& index){
				// ask for the d_k value here.
				// For every value im iterating over I need sth
					std::cout<<"Pos: "<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<std::endl;
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
