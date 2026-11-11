#pragma once

#include "../component.hpp"
#include "common.hpp"
#include "schema.hpp"
#include "porosity.hpp"
#include "cell_list.hpp"

namespace kel {
namespace lbm {

/**
 * Solid fraction at one lattice cell, summed over every particle covering it.
 *
 * This is the gather counterpart of the per-particle porosity stamp. The
 * stamp *writes* macros.porosity from a per-particle work item, so two
 * particles whose bounding boxes overlap race on the same cell -- and
 * contacting particles always overlap. Here one work item owns one cell and
 * writes only that cell, so there is nothing to race on and no atomic needed.
 *
 * Porosity composes as a product of the per-particle fluid fractions: a cell
 * fully inside either body is fully solid, and partial overlaps multiply
 * rather than clobbering one another. The per-particle version could not do
 * this at all -- whichever particle ran last simply won.
 */
template<typename T, uint64_t D, uint64_t CellCapacity, typename PartsRef, typename ClRef, typename CellRef>
kel::lbm::data<sch::Scalar<T>> cell_porosity(
	const PartsRef& parts,
	const ClRef& cl,
	const CellRef& cell,
	kel::lbm::data<sch::Scalar<T>> eps
){
	kel::lbm::data<sch::Scalar<T>> por;
	por.at({}).set(1);

	kel::lbm::data<sch::Vector<T,D>> cell_pos;
	for(uint64_t i{0u}; i < D; ++i){
		cell_pos.at({{i}}).set(
			static_cast<typename kel::lbm::native_data_type<T>::type>(cell.at({i}).get())
		);
	}

	for_each_bin_candidate<T,D,CellCapacity>(cl,cell,[&](uint64_t p){
		auto& pi = parts.at({p});
		auto& pos = pi.template get<"rigid_body">().template get<"position">();
		auto rad = pi.template get<"collision">().template get<"radius">().handle();

		kel::lbm::data<sch::Vector<T,D>> rel;
		for(uint64_t i{0u}; i < D; ++i){
			rel.at({{i}}).set(cell_pos.at({{i}}).get() - pos.at({{i}}).get());
		}

		auto p_por = particle_porosity<T,D,1u,por::ParticleSpheroid<T>>::calculate(rel,rad,eps);

		// 1 = fluid, 0 = solid, so fractions multiply.
		por.at({}).set(por.at({}).get() * p_por.at({}).get());
	});

	return por;
}

namespace cmpt {
/**
 * Writes the particle coupling fields for one lattice cell.
 *
 * Replaces the per-particle scatter. Runs over the lattice, not the particle
 * list, so it stays correct once particles start overlapping.
 */
template<uint64_t CellCapacity>
struct ParticleFluidCouple {};
}

template<typename T, typename Descriptor, uint64_t CellCapacity, typename Encode>
class component<T, Descriptor, cmpt::ParticleFluidCouple<CellCapacity>, Encode> final {
public:
	using Component = cmpt::ParticleFluidCouple<CellCapacity>;

private:
	kel::lbm::data<sch::Scalar<T>> eps_;

public:
	component(){
		eps_.at({}).set(1.5);
	}

	component(const kel::lbm::data<sch::Scalar<T>>& eps__):
		eps_{eps__}
	{}

	template<typename MacroFieldSchema, typename ParticleSchema, typename CellListSchema>
	void apply(
		const kel::lbm::data<MacroFieldSchema,Encode>& macros,
		const kel::lbm::data<ParticleSchema,Encode>& pg,
		const kel::lbm::data<CellListSchema,Encode>& cl,
		kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index,
		kel::lbm::data<sch::UInt64> time_step
	) const {
		(void) time_step;

		auto& parts = pg.template get<"particles">();

		macros.template get<"porosity">().at(index) =
			cell_porosity<T,Descriptor::D,CellCapacity>(parts,cl,index,eps_);
	}
};

}
}
