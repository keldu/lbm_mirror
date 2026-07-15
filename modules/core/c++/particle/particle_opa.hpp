#pragma once

#include "../component.hpp"
#include "common.hpp"
#include "schema.hpp"
#include "porosity.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct OneParticleAt {};
}

template<typename T, typename Descriptor, typename Encode>

class component<T,Descriptor,cmpt::OneParticleAt, Encode> final {
private:
	saw::data<sch::Vector<T,Descriptor::D>> pos_;
	saw::data<sch::Scalar<T>> rad_;
	saw::data<sch::Scalar<T>> eps_;
public:
	component(
		const saw::data<sch::Vector<T,Descriptor::D>> pos__,
		const saw::data<sch::Scalar<T>> rad__,
		const saw::data<sch::Scalar<T>> eps__
	):
		pos_{pos__},
		rad_{rad__},
		eps_{eps__}
	{}

	template<typename MacroFieldSchema>
	void apply(const saw::data<MacroFieldSchema, Encode>& macros, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<T,Descriptor>;

		auto& porous_f = macros.template get<"porosity">();
		auto& porous = porous_f.at(index);

		auto pos_ind = saw::math::vectorize_data(index);

		// Write out value
		auto diff = pos_ind.template cast_to<T>() - pos_;
		auto diff_dot = saw::math::dot(diff,diff);
		porous = particle_porosity<T,Descriptor::D,1u,por::ParticleSpheroid<T>>::calculate(diff, rad_, eps_);
	}
};
}
}
