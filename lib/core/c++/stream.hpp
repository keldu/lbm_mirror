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
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step) const {
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto info_f = field.template get<"info">();

		auto info_meta = info_f.get_dims();

		bool border = false;
		for(uint64_t i = 0u; i < Descriptor::D; ++i){
			auto ind_i = index.at({i});
			border |= (ind_i.get()) == 0u or (ind_i == info_meta.at({i}));
		}
	}
};
}
}
