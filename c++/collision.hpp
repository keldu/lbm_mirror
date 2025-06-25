#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BGK {};
}

template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::BGK> {
private:
	typename saw::native_data_type<T>::type relaxation_;
public:
	component(typename saw::native_data_type<T>::type relaxation__):
		relaxation_{relaxation__}
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
	void apply(saw::data<CellFieldSchema>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, uint64_t time_step){
		bool is_even = ((time_step % 2) == 0);
		auto& cell = field(index);

		auto& dfs_old = is_even ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		auto& dfs = (!is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		saw::data<T> rho;
		saw::data<sch::FixedArray<T,Descriptor::D>> vel;
		compute_rho_u<T,Descriptor>(dfs,rho,vel);
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old({i}).set(dfs_old({i}).get() + (1.0 / relaxation_) * (eq.at(i).get() - dfs_old({i}).get()));
		}
	}


};
}
}
