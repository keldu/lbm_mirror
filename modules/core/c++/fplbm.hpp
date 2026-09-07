#pragma once

#include "common.hpp"
#include "particle/particle.hpp"
#include "iterator.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct FpLbmReset{};
struct FpLbm {};
struct FpLbmOneParticle {};
struct FpLbmMomentumOneParticle {};
struct FpLbmOneParticleNoVelocity{};
}
namespace method {
struct FpLbm {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::FpLbmReset, Encode> final {
public:
	using Component = cmpt::FpLbmReset;
private:
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		auto& por_f = macros.template get<"porosity">();

		auto& por = por_f.at(index);
		por.at({}) = 1.0;

		auto& force_f = macros.template get<"force">();
		auto& force = force_f.at(index);
		for(uint64_t i{0u}; i < Descriptor::D; ++i){
			force.at({{i}}) = 0.0;
		}
		auto info = field.template get<"info">().at(index).get();
		if(info == 2u){
			bool is_even = ((time_step.get() % 2) == 0);

			auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
			auto& dfs = dfs_old_f.at(index);

			auto& rho_f = macros.template get<"density">();
			auto& rho = rho_f.at(index);
			auto& vel_f = macros.template get<"momentum">();
			auto& vel = vel_f.at(index);

			compute_rho_momentum<T,Descriptor>(dfs,rho,vel);
		}
	}
};

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::FpLbm, Encode> final {
private:
	saw::data<T> relaxation_;
	saw::data<T> frequency_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{{relaxation__}},
		frequency_{saw::data<T>{1} / relaxation_}
	{}

	component(const saw::data<T>& relaxation__):
		relaxation_{relaxation__},
		frequency_{saw::data<T>{1} / relaxation_}
	{}

	using Component = cmpt::FpLbm;


	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs = dfs_old_f.at(index);

		saw::data<sch::Scalar<T>> half;
		half.at({}) = 0.5f;

		auto& rho_f = macros.template get<"density">();
		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);

		auto& force_f = macros.template get<"force">();
		auto& force = force_f.at(index);

		auto& vel_f = macros.template get<"momentum">();
		saw::data<sch::Vector<T,Descriptor::D>> vel = vel_f.at(index);

		// We already computed this in reset
		// compute_rho_u<T,Descriptor>(dfs,rho,vel);
		vel = vel + force * half;
		auto eq = equilibrium<T,Descriptor>(rho,vel / rho);

		using dfi = df_info<T,Descriptor>;

		auto& por_f = macros.template get<"porosity">();
		auto& por = por_f.at(index);

		saw::data<sch::Scalar<T>> dfi_inv_cs2;
		dfi_inv_cs2.at({}).set(dfi::inv_cs2);

		for(uint64_t i{0u}; i < Descriptor::Q; ++i){
			saw::data<sch::Vector<T,Descriptor::D>> ci;
			for(uint64_t d{0u}; d < Descriptor::D; ++d){
				ci.at({{d}}).set(static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d]));
			}
			auto ci_dot_u = saw::math::dot(ci,vel/rho);

			saw::data<sch::Scalar<T>> w;
			w.at({}).set(dfi::weights[i]);

			auto term1 = (ci-vel/rho) * dfi_inv_cs2;
			auto term2 = ci * (ci_dot_u * dfi_inv_cs2 * dfi_inv_cs2);

			auto force_projection = saw::math::dot(term1 + term2, force);

			auto F_i = w * force_projection;

			dfs.at({i}) = dfs.at({i})
									+ frequency_
										* (eq.at(i) - dfs.at({i}) )
									+ F_i.at({})
										* (saw::data<T>{1} - saw::data<T>{0.5f} * frequency_);
		}
	}
};

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::FpLbmMomentumOneParticle, Encode> final {
public:
	using Component = cmpt::FpLbmOneParticle;
private:
	saw::data<sch::Vector<T,Descriptor::D>> global_force_;
public:
	component():
		global_force_{}
	{}
	component(saw::data<sch::Vector<T,Descriptor::D>> global_force__):
		global_force_{global_force__}
	{}
	
	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& part_group, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step, saw::data<sch::UInt64> sub_steps) const {
		/// Figure out how to access the particle list
		// auto& p = particles.at(i);

		/// Iterate over the grid bounds
		// auto& grid = p.template get<"grid">();
		using dfi = df_info<T,Descriptor>;
		bool is_even = ((time_step.get() % 2) == 0);
		
		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& part_spheroid_group = part_group;
		auto& mvel_f = macros.template get<"momentum">();
		auto& mrho_f = macros.template get<"density">();
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
			saw::data<sch::Vector<T,Descriptor::D>>& pirb_pos = pirb.template get<"position">();
			auto& pirb_pos_old = pirb.template get<"position_old">();
				
			// TODO !!!! Divide by actual time step - for now it's ok
			saw::data<sch::Scalar<T>> ts;
			ts.at({}) = one.at({}) / sub_steps.template cast_to<T>();

			saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> start;
			saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> stop;

			auto eo_aabb = particle_aabb<typename ParticleSchema::ValueType>::calculate(part_spheroid_group,{{0u}},mvel_f.meta());
			if(eo_aabb.is_error()){
				return;
			}
			auto& aabb = eo_aabb.get_value();

			/// Ok, I iterate over the space which covers our particle? So lower bounds to upper bounds
			start = aabb.template get<"a">();
			stop = aabb.template get<"b">();

			saw::data<sch::Vector<T,Descriptor::D>> force_p;
			for(uint64_t i{0u}; i < Descriptor::D; ++i){
				force_p.at({{i}}) = 0.0;
			}
			auto vel_p_old = (pirb_pos-pirb_pos_old) / ts;

			iterator<Descriptor::D>::apply([&](const auto& index_f) -> void{
				auto& dfs = dfs_old_f.at(index_f);
				saw::data<sch::Vector<T,Descriptor::D>> rel_dist = saw::math::vectorize_data(index_f).template cast_to<T>() - pirb_pos;
				saw::data<sch::Scalar<T>> eps;
				eps.at({}) = 1.5f;
				
				auto& mpor = mpor_f.at(index_f);
				mpor = particle_porosity<T,Descriptor::D,1u,por::ParticleSpheroid<T>>::calculate(rel_dist,p_rad,eps);

				if(mpor.at({}).get() >= 1.0f){
					return;
				}

				saw::data<sch::Vector<T,Descriptor::D>> momentum;
				for(uint64_t i{0u}; i < Descriptor::D; ++i){
					momentum.at({{i}}) = 0.0;
				}

				for(uint64_t i{0u}; i < Descriptor::Q; ++i){
					saw::data<sch::Vector<T,Descriptor::D>> e_i;
					saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> n_ind_i;
					for(uint64_t k{0u}; k < Descriptor::D; ++k){
						e_i.at({{k}}) = (dfi::directions[i])[k];
						n_ind_i.at({k}) = index_f.at({k}) + (dfi::directions[i])[k];
					}

					uint64_t i_opp = dfi::opposite_index[i];

					auto u_p_e = saw::math::dot(e_i,vel_p_old);

					saw::data<T> dfs_added = dfs.at({i})*(saw::data<T>{1}-u_p_e.at({})) + dfs_old_f.at(n_ind_i).at({i_opp})*(saw::data<T>{1}+u_p_e.at({}));
					saw::data<sch::Scalar<T>> dfs_added_v;
					dfs_added_v.at({}) = dfs_added;
					auto ei_dfs = e_i * dfs_added_v;

					momentum = momentum + ei_dfs;
				}

				// TODO technically needs to adjust for rotation as well

				auto& rho = mrho_f.at(index_f);

				macros.template get<"force">().at(index_f) = momentum;
				force_p = force_p + momentum;
			},start,stop);
			
			auto& pirb_acc = pirb.template get<"acceleration">();
			pirb_acc = - force_p / part_spheroid_group.template get<"total_mass">().at({});

			for(saw::data<sch::UInt64> i{0u}; i < sub_steps; ++i){
				verlet_step_lambda<T,Descriptor::D>(pi,ts);
			}
			auto vel_p = (pirb_pos-pirb_pos_old) / ts;

			iterator<Descriptor::D>::apply([&](const auto& index_f){
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

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::FpLbmOneParticle, Encode> final {
public:
	using Component = cmpt::FpLbmOneParticle;
private:
	saw::data<sch::Vector<T,Descriptor::D>> global_force_;
public:
	component():
		global_force_{}
	{}
	component(saw::data<sch::Vector<T,Descriptor::D>> global_force__):
		global_force_{global_force__}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, const saw::data<ParticleSchema,Encode>& pg, saw::data<sch::FixedArray<sch::UInt64,1u>> index, saw::data<sch::UInt64> time_step, saw::data<sch::UInt64> sub_steps) const {
	// void apply(saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>> index, saw::data<sch::UInt64> time_step){
		bool is_even = ((time_step.get() % 2) == 0);

		//auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		// auto& dfs = dfs_old_f.at(index);

		auto& rho_f = macros.template get<"density">();
		auto& mom_f = macros.template get<"momentum">();
		auto& por_f = macros.template get<"porosity">();
		auto& force_f = macros.template get<"force">();

		/**
		 * The other methods all work with a flipped porosity.
		 * For compat reasons this is also flipped
		 */
		// TODO - Change to tuple later
		auto parts = pg.template get<"particles">();

		auto parts_size = parts.meta().at({0u});
		auto& pi = parts.at(index);		

		auto& pirb = pi.template get<"rigid_body">();
		auto& pirb_pos = pirb.template get<"position">();
		auto& pirb_pos_old = pirb.template get<"position_old">();
		

		auto& p_coll = pg.template get<"collision">().at({});
		auto& p_rad = p_coll.template get<"radius">();

		auto eo_aabb = particle_aabb<typename ParticleSchema::ValueType>::calculate(pg,{{0u}},mom_f.meta());
		if(eo_aabb.is_error()){
			return;
		}
		auto& aabb = eo_aabb.get_value();

		saw::data<sch::Scalar<T>> three;
		three.at({}).set(3.0f);

		saw::data<sch::Scalar<T>> two;
		two.at({}).set(2.0f);

		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0f;
		
		saw::data<sch::Scalar<T>> eps;
		eps.at({}) = 1.5f;
		
		saw::data<sch::Scalar<T>> sss;
		sss.at({}) = sub_steps.template cast_to<T>();
		auto vel_s = (pirb_pos-pirb_pos_old) * sss;

		saw::data<sch::Vector<T,Descriptor::D>> force_p = global_force_;

		iterator<Descriptor::D>::apply([&](const auto& index_f) -> void {
			auto& force = force_f.at(index_f);
			auto& mom = mom_f.at(index_f);
			auto& por = por_f.at(index_f);
			auto& rho = rho_f.at(index_f);

			saw::data<sch::Vector<T,Descriptor::D>> rel_dist = saw::math::vectorize_data(index_f).template cast_to<T>() - pirb_pos;

			por = particle_porosity<T,Descriptor::D,1u,por::ParticleSpheroid<T>>::calculate(rel_dist,p_rad,eps);
			auto flip_por = one - por;

			// vel_s is technically time the density of the particle?

			force = global_force_ * por + ( vel_s - mom) * two * flip_por;
			// force = (vel_s - mom) * flip_por / (flip_por / two + one);

			force_p = force_p - force;
		}, aabb.template get<"a">(), aabb.template get<"b">());

		auto& pirb_acc = pirb.template get<"acceleration">();
		pirb_acc = force_p / (pg.template get<"total_mass">().at({}));

		for(saw::data<sch::UInt64> i{0u}; i < sub_steps; ++i){
			verlet_step_lambda<T,Descriptor::D>(pi,one / sss);
		}
	}
};
}
}
