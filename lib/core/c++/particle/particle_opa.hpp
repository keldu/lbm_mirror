#pragma once

#include "component.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct OneParticleAt {};
}

template<typename T, typename Descriptor, typename Encode>

class component<T,Descriptor,cmpt::OneParticleAt, Encode> final {
private:
public:
	component() = default;

	template<typename MacroFieldSchema>
	void apply(const saw::data<MacroFieldSchema, Encode>& macros, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<T,Descriptor>;

		auto& porous_f = macros.template get<"porosity">();

		auto& porous = porous_f.at(index);


	}
};
}
}
