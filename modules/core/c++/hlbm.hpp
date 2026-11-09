#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

#include "particle/particle.hpp"
#include "particle/integrate.hpp"

#include <iostream>

namespace kel {
namespace lbm {
namespace cmpt {
struct HlbmReset {}; 
struct Hlbm {};
struct HlbmOneParticle {};
struct HlbmParticle {};
struct HlbmOneParticleMomentumExchange {};
struct HlbmOneParticleGather {};
struct HlbmOneParticleStamp {};
}
namespace method {
struct Hlbm {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::HlbmReset, Encode> final {
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, kel::lbm::data<sch::UInt64> time_step) const {
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
	typename kel::lbm::native_data_type<T>::type relaxation_;
	kel::lbm::data<T> frequency_;
public:
	component(typename kel::lbm::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename kel::lbm::native_data_type<T>::type(1) / relaxation_}
	{}

	component(const kel::lbm::data<sch::Pure<T>>& relaxation__):
		relaxation_{relaxation__.handle().get()},
		frequency_{typename kel::lbm::native_data_type<T>::type(1) / relaxation_}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> index, kel::lbm::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto& porosity_f = macros.template get<"porosity">();
		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"momentum">();

		kel::lbm::data<sch::Scalar<T>>& rho = rho_f.at(index);
		kel::lbm::data<sch::Vector<T,Desc::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Desc>(dfs_old_f.at(index), rho, vel);

		auto& porosity = porosity_f.at(index);
		kel::lbm::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		auto flip_porosity = one - porosity;

		auto& N = particle_N_f.at(index);
		auto& D = particle_D_f.at(index);

		// Convex combination of velocities
		vel = vel * porosity + [&]() -> kel::lbm::data<sch::Vector<T,Desc::D>> {
			return (D.at({}).get() > 0.0) ? ( N * flip_porosity / D ): N;
		}();
		// Equilibrium
		auto eq = equilibrium<T,Desc>(rho,vel);

		for(uint64_t i = 0u; i < Desc::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}

	}
};
/**
 * Gathers the hydrodynamic force on one particle and stamps its porosity.
 *
 * Accumulates into the particle's force member rather than writing
 * acceleration, so contact forces can be added before cmpt::ParticleIntegrate
 * turns the total into motion.
 */
template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmOneParticleGather, Encode> final {
private:
public:
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, const kel::lbm::data<ParticleSchema,Encode>& part_group, kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index, kel::lbm::data<sch::UInt64> time_step, kel::lbm::data<sch::UInt64> sub_steps) const {
		using dfi = df_info<T,Desc>;
		bool is_even = ((time_step.get() % 2) == 0);

		kel::lbm::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& part_spheroid_group = part_group;
		auto& mvel_f = macros.template get<"momentum">();
		auto& mrho_f = macros.template get<"density">();
		auto& mpor_f = macros.template get<"porosity">();
		{
			auto parts = part_spheroid_group.template get<"particles">();

			auto& pi = parts.at(index);
			auto& pirb = pi.template get<"rigid_body">();
			auto& p_rad = pi.template get<"collision">().template get<"radius">();
			kel::lbm::data<sch::Vector<T,Desc::D>>& pirb_pos = pirb.template get<"position">();
			auto& pirb_pos_old = pirb.template get<"position_old">();

			// Matches cmpt::ParticleIntegrate: zero sub steps means one step.
			uint64_t steps = sub_steps.get();
			if(steps == 0u){
				steps = 1u;
			}
			kel::lbm::data<sch::Scalar<T>> ts;
			ts.at({}).set(
				typename kel::lbm::native_data_type<T>::type(1)
				/ static_cast<typename kel::lbm::native_data_type<T>::type>(steps)
			);

			kel::lbm::data<typename particle_aabb<typename ParticleSchema::ValueType>::AABB> aabb;
			if(not particle_aabb<typename ParticleSchema::ValueType>::try_calculate(part_spheroid_group,index,mvel_f.meta(),aabb)){
				return;
			}

			auto start = aabb.template get<"a">();
			auto stop = aabb.template get<"b">();

			kel::lbm::data<sch::Vector<T,Desc::D>> force_p;
			for(uint64_t i{0u}; i < Desc::D; ++i){
				force_p.at({{i}}) = 0.0;
			}
			auto vel_p_old = (pirb_pos-pirb_pos_old) / ts;

			iterator<Desc::D>::apply([&](const auto& index_f) -> void{
				auto& dfs = dfs_old_f.at(index_f);
				kel::lbm::data<sch::Vector<T,Desc::D>> rel_dist = kel::lbm::math::vectorize_data(index_f).template cast_to<T>() - pirb_pos;
				kel::lbm::data<sch::Scalar<T>> eps;
				eps.at({}) = 1.5f;

				auto& mpor = mpor_f.at(index_f);
				mpor = particle_porosity<T,Desc::D,1u,por::ParticleSpheroid<T>>::calculate(rel_dist,p_rad,eps);

				if(mpor.at({}).get() >= 1.0f){
					return;
				}

				kel::lbm::data<sch::Vector<T,Desc::D>> momentum;
				for(uint64_t i{0u}; i < Desc::D; ++i){
					momentum.at({{i}}) = 0.0;
				}

				for(uint64_t i{0u}; i < Desc::Q; ++i){
					kel::lbm::data<sch::Vector<T,Desc::D>> e_i;
					kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> n_ind_i;
					for(uint64_t k{0u}; k < Desc::D; ++k){
						e_i.at({{k}}) = (dfi::directions[i])[k];
						n_ind_i.at({k}) = index_f.at({k}) + (dfi::directions[i])[k];
					}

					uint64_t i_opp = dfi::opposite_index[i];

					auto u_p_e = kel::lbm::math::dot(e_i,vel_p_old);

					kel::lbm::data<T> dfs_added = dfs.at({i})*(kel::lbm::data<T>{1}-u_p_e.at({})) + dfs_old_f.at(n_ind_i).at({i_opp})*(kel::lbm::data<T>{1}+u_p_e.at({}));
					kel::lbm::data<sch::Scalar<T>> dfs_added_v;
					dfs_added_v.at({}) = dfs_added;
					auto ei_dfs = e_i * dfs_added_v;

					momentum = momentum + ei_dfs;
				}

				// TODO technically needs to adjust for rotation as well

				auto& rho = mrho_f.at(index_f);
				(void) rho;

				macros.template get<"force">().at(index_f) = momentum;
				force_p = force_p + momentum;
			},start,stop);

			// Accumulate, do not assign: contact adds to this before integration.
			auto& pirb_force = pirb.template get<"force">();
			pirb_force = pirb_force - force_p;
		}
	}
};

/**
 * Writes the HLBM solid-fraction coupling terms.
 *
 * Must run AFTER cmpt::ParticleIntegrate, because it uses the post-move
 * particle velocity.
 */
template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmOneParticleStamp, Encode> final {
private:
public:
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, const kel::lbm::data<ParticleSchema,Encode>& part_group, kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index, kel::lbm::data<sch::UInt64> time_step, kel::lbm::data<sch::UInt64> sub_steps) const {
		(void) time_step;

		kel::lbm::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;

		auto& mvel_f = macros.template get<"momentum">();
		auto& mpor_f = macros.template get<"porosity">();
		auto& particle_N_f = field.template get<"particle_N">();
		auto& particle_D_f = field.template get<"particle_D">();

		auto parts = part_group.template get<"particles">();
		auto& pi = parts.at(index);
		auto& pirb = pi.template get<"rigid_body">();
		auto& pirb_pos = pirb.template get<"position">();
		auto& pirb_pos_old = pirb.template get<"position_old">();

		uint64_t steps = sub_steps.get();
		if(steps == 0u){
			steps = 1u;
		}
		kel::lbm::data<sch::Scalar<T>> ts;
		ts.at({}).set(
			typename kel::lbm::native_data_type<T>::type(1)
			/ static_cast<typename kel::lbm::native_data_type<T>::type>(steps)
		);

		kel::lbm::data<typename particle_aabb<typename ParticleSchema::ValueType>::AABB> aabb;
		if(not particle_aabb<typename ParticleSchema::ValueType>::try_calculate(part_group,index,mvel_f.meta(),aabb)){
			return;
		}

		auto vel_p = (pirb_pos-pirb_pos_old) / ts;

		iterator<Desc::D>::apply([&](const auto& index_f){
			auto& N = particle_N_f.at(index_f);
			auto& D = particle_D_f.at(index_f);
			auto& mpor = mpor_f.at(index_f);
			auto flip_porosity = one - mpor;

			D = D + flip_porosity;
			N = N + vel_p * flip_porosity;
		},aabb.template get<"a">(),aabb.template get<"b">());
	}
};

/**
 * The original fused one-particle step, kept so existing call sites keep
 * working: gather, integrate, then stamp, in that order.
 *
 * New code should launch the three components separately, which is what makes
 * room for a contact pass between the gather and the integrate.
 */
template<typename T, typename Desc, typename Encode>
class component<T, Desc, cmpt::HlbmOneParticle, Encode> final {
private:
public:
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, const kel::lbm::data<ParticleSchema,Encode>& part_group, kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index, kel::lbm::data<sch::UInt64> time_step, kel::lbm::data<sch::UInt64> sub_steps) const {
		component<T,Desc,cmpt::ParticleForceReset,Encode> reset;
		component<T,Desc,cmpt::HlbmOneParticleGather,Encode> gather;
		component<T,Desc,cmpt::ParticleIntegrate,Encode> integrate;
		component<T,Desc,cmpt::HlbmOneParticleStamp,Encode> stamp;

		reset.apply(part_group,index,time_step);
		gather.apply(field,macros,part_group,index,time_step,sub_steps);
		integrate.apply(part_group,index,time_step,sub_steps);
		stamp.apply(field,macros,part_group,index,time_step,sub_steps);
	}
};
}
}
