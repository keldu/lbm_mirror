#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BounceBack {};

template<bool East>
struct ZouHeHorizontal{};

struct Equilibrium {};

template<bool North>
struct ZouHeVertical{};
}

/**
 * Full-Way BounceBack.
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::BounceBack, Encode> {
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
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step)const{
		bool is_even = ((time_step.get() % 2u) == 0u);

		// This is a ref

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		using dfi = df_info<T,Descriptor>;

		// Technically use .copy()
		auto& dfs_old = dfs_old_f.at(index);
		auto df_cpy = dfs_old;

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old.at({i}) = df_cpy.at({dfi::opposite_index.at(i)});
		}
	}
};

template<typename FP, typename Descriptor, typename Encode>
class component<FP, Descriptor, cmpt::Equilibrium, Encode> final {
private:
	saw::data<FP> density_;
	saw::data<FP> velocity_;
public:
	component(
		saw::data<FP> density__,
		saw::Data<FP> velocity__
	):
		density_{density__},
		velocity_{velocity__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step)const{
		using dfi = df_info<T,Descriptor>;
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
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
template<typename FP, typename Descriptor, bool Dir, typename Encode>
class component<FP, Descriptor, cmpt::ZouHeHorizontal<Dir>, Encode> final {
private:
	saw::data<FP> rho_setting_;
public:
	component(const saw::data<FP>& density_setting__):
		rho_setting_{density_setting__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<FP,Descriptor>;
		constexpr int known_dir = Dir ? 1 : -1;

		bool is_even = ((time_step.get() % 2) == 0);

		auto& info_f = field.template get<"info">();
		
		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& dfs_old = dfs_old_f.at(index);
		/**
		 * Sum all known DFs
		 */
		saw::data<FP> sum_df{0};
		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Descriptor::Q}; ++k){
			auto c_k = dfi::directions[k.get()];

			if(c_k[0u]*known_dir <= 0){
				sum_df += dfs_old.at({k});
			}
		}

		/**
		 * Get the sum of the unknown dfs and precalculate the direction
		 */
		auto sum_unknown_dfs = (rho_setting_ - sum_df) * saw::data<FP>{known_dir};

		for(saw::data<sch::UInt64> k{0u}; k < saw::data<sch::UInt64>{Descriptor::Q}; ++k){
			auto c_k = dfi::directions[k.get()];
			if(c_k[0u]*known_dir < 0){
				sum_unknown_dfs += dfs_old.at({k}) * c_k[0u];
			}
		}

		auto vel_x = sum_unknown_dfs / rho_setting_;

		static_assert(Descriptor::D == 2u and Descriptor::Q == 9u, "Some parts are hard coded sadly");

		if constexpr (Dir) {
			dfs_old.at({2u}) = dfs_old.at({1u}) + saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old.at({6u}) = dfs_old.at({5u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old.at({4u}) - dfs_old.at({3u}));
			dfs_old.at({8u}) = dfs_old.at({7u}) + saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old.at({3u}) - dfs_old.at({4u}));
		}else if constexpr (not Dir){
			dfs_old.at({1u}) = dfs_old.at({2u}) - saw::data<FP>{2.0 / 3.0} * rho_setting_ * vel_x;
			dfs_old.at({5u}) = dfs_old.at({6u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old.at({3u}) - dfs_old.at({4u}));
			dfs_old.at({7u}) = dfs_old.at({8u}) - saw::data<FP>{1.0 / 6.0} * rho_setting_ * vel_x + saw::data<FP>{0.5} * (dfs_old.at({4u}) - dfs_old.at({3u}));
		}
	}
};
}
}
