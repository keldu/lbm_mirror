#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"
#include "chunk.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BGK {};
struct BGKGuo {};
}

/**
 * Standard BGK collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::BGK, Encode> {
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

	using Component = cmpt::BGK;

	/**
	 * Thoughts regarding automating order setup
	 */
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "collision";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		bool is_even = ((time_step.get() % 2) == 0);

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		saw::data<sch::Scalar<T>> rho;
		saw::data<sch::Vector<T,Descriptor::D>> vel;
		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}
	}
	
	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		bool is_even = ((time_step.get() % 2) == 0);

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"velocity">();
	
		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old_f.at(index).at({i}) = dfs_old_f.at(index).at({i}) + frequency_ * (eq.at(i) - dfs_old_f.at(index).at({i}));
		}
	}
};

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::BGKGuo, Encode> {
private:
	typename saw::native_data_type<T>::type relaxation_;
	saw::data<T> frequency_;
	saw::data<sch::Vector<T,Descriptor::D>> global_force_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_},
		global_force_{}
	{}

	component(
		typename saw::native_data_type<T>::type relaxation__,
		saw::data<sch::Vector<T,Descriptor::D>>& global_force__
	):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_},
		global_force_{global_force__}
	{}

	using Component = cmpt::BGKGuo;

	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>,
		cmpt::access<"force", 0, true, false>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "collision";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

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

			auto F_i_d = saw::math::dot((force+global_force_) * w, (ci - vel * dfi_inv_cs2 + ci * ci_dot_u * dfi_inv_cs2 * dfi_inv_cs2 ));
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
}
}
