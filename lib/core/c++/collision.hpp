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
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__},
		frequency_{typename saw::native_data_type<T>::type(1) / relaxation_}
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

	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64, Descriptor::D>> index, saw::data<sch::UInt64> time_step){
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs = dfs_old_f.at(index);

		saw::data<sch::Scalar<T>> rho;
		saw::data<sch::Vector<T,Descriptor::D>> vel;
		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		using dfi = df_info<T,Descriptor>;

		auto& force_f = field.template get<"force">();
		auto& force = force_f.at(index);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			// saw::data<T> ci_min_u{0};
			saw::data<T> ci_dot_u{0};
			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				ci_dot_u = ci_dot_u + vel.at({{d}}) * saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])};
			}
			saw::data<T> F_i{0};// = saw::data<T>{dfi::weights[i]} * (ci_dot_F * dfi::inv_cs2 + ci_dot_u * ci_dot_F * (dfi::inv_cs2 * dfi::inv_cs2));

			for(uint64_t d = 0u; d < Descriptor::D; ++d){
				F_i = F_i +
					saw::data<T>{dfi::weights[i]} * ((saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])}
					- vel.at({d}) ) * dfi::inv_cs2 + ci_dot_u * saw::data<T>{static_cast<typename saw::native_data_type<T>::type>(dfi::directions[i][d])} * dfi::inv_cs2  * dfi::inv_cs2 ) * force.at({d});
			}


			dfs.at({i}) = dfs.at({i}) + frequency_ * (eq.at(i) - dfs.at({i}) ) + F_i * (saw::data<T>{1} - saw::data<T>{0.5f} * frequency_);
		}
	}
};
}
}
