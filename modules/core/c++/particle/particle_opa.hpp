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
	kel::lbm::data<sch::Vector<T,Descriptor::D>> pos_;
	kel::lbm::data<sch::Scalar<T>> rad_;
	kel::lbm::data<sch::Scalar<T>> eps_;
public:
	component(
		const kel::lbm::data<sch::Vector<T,Descriptor::D>> pos__,
		const kel::lbm::data<sch::Scalar<T>> rad__,
		const kel::lbm::data<sch::Scalar<T>> eps__
	):
		pos_{pos__},
		rad_{rad__},
		eps_{eps__}
	{}

	template<typename MacroFieldSchema>
	void apply(const kel::lbm::data<MacroFieldSchema, Encode>& macros, const kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, kel::lbm::data<sch::UInt64> time_step) const {
		using dfi = df_info<T,Descriptor>;

		auto& porous_f = macros.template get<"porosity">();
		auto& porous = porous_f.at(index);

		auto pos_ind = kel::lbm::math::vectorize_data(index);

		// Write out value
		auto diff = pos_ind.template cast_to<T>() - pos_;
		auto diff_dot = kel::lbm::math::dot(diff,diff);
		porous = particle_porosity<T,Descriptor::D,1u,por::ParticleSpheroid<T>>::calculate(diff, rad_, eps_);
	}
};
}
}
