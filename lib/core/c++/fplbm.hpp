#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct FpLbm {};
struct FpLbmOneParticleNoVelocity{};
}

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
	// void apply(saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>> index, saw::data<sch::UInt64> time_step){
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs = dfs_old_f.at(index);

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();

		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		using dfi = df_info<T,Descriptor>;

		auto& force_f = macros.template get<"force">();
		auto& force = force_f.at(index);

		auto& porosity_f = macros.template get<"porosity">();
		auto& porosity = porosity_f.at(index);

		saw::data<sch::Scalar<T>> dfi_inv_cs2;
		dfi_inv_cs2.at({}).set(dfi::inv_cs2);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			// saw::data<T> ci_min_u{0};
			saw::data<sch::Vector<T,Descriptor::D>> ci;
			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				ci.at({{d}}).set(static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d]));
			}
			auto ci_dot_u = saw::math::dot(ci,vel);

			// saw::data<sch::Vector<T,Descriptor::D>> F_i;
			// F_i = f * (c_i - u * ics2  + <c_i,u> * c_i * ics2 * ics2) * w_i;
			saw::data<sch::Scalar<T>> w;
			w.at({}).set(dfi::weights[i]);

			auto F_i_d = saw::math::dot(force * w, (ci - vel * dfi_inv_cs2 + ci * ci_dot_u * dfi_inv_cs2 * dfi_inv_cs2 ));
			/*
			saw::data<sch::Scalar<T>> F_i_sum;
			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				saw::data<sch::Scalar<T>> F_i_d;
				F_i_d.at({}) = F_i.at({{d}});
				F_i_sum = F_i_sum + F_i_d;
			}
			*/

			dfs.at({i}) = dfs.at({i}) + frequency_ * (eq.at(i) - dfs.at({i}) ) + F_i_d.at({}) * (saw::data<T>{1} - saw::data<T>{0.5f} * frequency_);
		}
	}
};

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::FpLbmOneParticleNoVelocity, Encode> final {
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

	using Component = cmpt::FpLbmOneParticleNoVelocity;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
	// void apply(saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>> index, saw::data<sch::UInt64> time_step){
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs = dfs_old_f.at(index);

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();
		auto& por_f = macros.template get<"porosity">();

		// Temporary find a better way
		auto& force_f = macros.template get<"force">();


		/**
		 * The other methods all work with a flipped porosity.
		 * For compat reasons this is also flipped
		 */
		auto porosity = por_f.at(index);
		saw::data<sch::Scalar<T>> one;
		one.at({}) = 1.0;
		auto flip_porosity = one - porosity;

		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);

		saw::data<sch::Scalar<T>> half;
		half.at({}).set(0.5);
		saw::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);// + total_force * ( half / rho );

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		using dfi = df_info<T,Descriptor>;

		saw::data<sch::Scalar<T>> min_two;
		min_two.at({}).set(-2);

		// Maybe ? 
		auto& force = force_f.at(index);
		force = vel * rho * min_two * flip_porosity;

		saw::data<sch::Scalar<T>> dfi_inv_cs2;
		dfi_inv_cs2.at({}).set(dfi::inv_cs2);

		// auto vel = vel_f.at(index);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			// saw::data<T> ci_min_u{0};
			saw::data<sch::Vector<T,Descriptor::D>> ci;
			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				ci.at({{d}}).set(static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d]));
			}
			auto ci_dot_u = saw::math::dot(ci,vel);

			// saw::data<sch::Vector<T,Descriptor::D>> F_i;
			// F_i = f * ((c_i - u) * ics2  + <c_i,u> * c_i * ics2 * ics2) * w_i;
			saw::data<sch::Scalar<T>> w;
			w.at({}).set(dfi::weights[i]);

			/*
			saw::data<sch::Scalar<T>> F_i_sum;
			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				saw::data<sch::Scalar<T>> F_i_d;
				F_i_d.at({}) = F_i.at({{d}});
				F_i_sum = F_i_sum + F_i_d;
			}
			*/
			auto term1 = (ci-vel) * dfi_inv_cs2;
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
}
}
