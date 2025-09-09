#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BounceBack {};

template<bool East>
struct ZouHeHorizontal{};

template<bool North>
struct ZouHeVertical{};
}

/**
 * Full-Way BounceBack.
 */
template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::BounceBack> {
private:
public:
	component() = default;

	using Component = cmpt::BounceBack;

	/**
	 * Thoughts regarding automating order setup
	 */
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "full_way_bounce_back";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(saw::data<CellFieldSchema>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, uint64_t time_step){
		bool is_even = ((time_step % 2u) == 0u);

		// This is a ref
		auto& cell = field(index);

		auto& dfs_old = (is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();
		// auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

		using dfi = df_info<T,Descriptor>;

		// Technically use .copy()
		auto df_cpy = dfs_old.copy();

		for(uint64_t i = 1u; i < Descriptor::Q; ++i){
			dfs_old({i}) = df_cpy({dfi::opposite_index.at(i)});
		}
	}
};

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
template<typename FP, typename Descriptor, bool Dir>
class component<FP, Descriptor, cmpt::ZouHeHorizontal<Dir>> final {
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
		// auto& dfs = (not is_even) ? cell.template get<"dfs_old">() : cell.template get<"dfs">();

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
		constexpr int known_dir = Dir ? 1 : -1;
		auto sum_unknown_dfs = (rho_setting_ - sum_df) * saw::data<FP>{known_dir};

		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Descriptor::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			auto& cell_n = field({{index.at({0u})+c_k[0u], index.at({1u})+c_k[1u]}});
			auto& info_n = cell_n.template get<"info">();
			auto info_n_val = info_n({0u});
			// auto k_opp = dfi::opposite_index[k.get()];

			if(info_n_val.get() > 0u){
				sum_unknown_dfs += dfs_old({k}) * c_k[0u];
			}
		}

		auto vel_x = sum_unknown_dfs / rho_setting_;

		static_assert(Descriptor::D == 2u and Descriptor::Q == 9u, "Some parts are hard coded sadly");

		if constexpr (Dir) {
			dfs_old({2u}) = dfs_old({1u}) + saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({6u}) = dfs_old({5u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
			dfs_old({8u}) = dfs_old({7u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
		}else if constexpr (not Dir){
			dfs_old({1u}) = dfs_old({2u}) - saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old({5u}) = dfs_old({6u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({3u}) - dfs_old({4u}));
			dfs_old({7u}) = dfs_old({8u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old({4u}) - dfs_old({3u}));
		}
	}
};
}
}
