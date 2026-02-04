#pragma once

#include "component.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct Stream {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T,Descriptor, cmpt::Stream, Encode> final {
private:
public:
	static constexpr saw::string_literal name = "streaming";
	static constexpr saw::string_literal after = "collide";
	static constexpr saw::string_literal before = "";

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<T,Descriptor>;
		auto g_index = index;

		for(uint64_t i = 0u; i < Descriptor::D; ++i){
			++g_index.at({{i}});
		}

		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& dfs_new_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto info_f = field.template get<"info">();

		auto info_meta = info_f.get_dims();

		/*
		bool border = false;
		for(uint64_t i = 0u; i < Descriptor::D; ++i){
			auto ind_i = index.at({i});
			border |= (ind_i.get()) == 0u or (ind_i == info_meta.at({i}));
		}

		if (not border){
		}
		*/

		auto& dfs_new = dfs_new_f.ghost_at(g_index);

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			auto dir = dfi::directions[dfi::opposite_index[i]];
			auto g_index_nb = g_index;
			for(uint64_t z = 0u; z < Descriptor::D; ++z){
				g_index_nb.at({z}).set(g_index.at({z}).get() + dir[z]);
			}

			dfs_new.at({i}) = dfs_old_f.ghost_at(g_index_nb).at({i});
		}
	}
};
}
}
