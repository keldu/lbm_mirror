#pragma once

#include "../component.hpp"
#include "../iterator.hpp"
#include "common.hpp"
#include "schema.hpp"
#include "particle.hpp"

namespace kel {
namespace lbm {

namespace sch {
/**
 * Uniform bin grid over the lattice, used to answer "which particles can
 * cover this cell" without scanning every particle.
 *
 * Bin size must be at least the largest particle diameter. Then a particle
 * covering a lattice cell is always registered in that cell's own bin or one
 * of its immediate neighbours, so a query only ever touches 3^D bins.
 *
 * `count` and `slot` are flat because that is the layout the SYCL copy helper
 * handles as a single memcpy; `slot` holds CellCapacity ids per bin.
 */
template<typename T, uint64_t D, uint64_t CellCapacity>
using ParticleCellList = Struct<
	Member<FixedArray<Scalar<T>,1u>, "bin_size">,
	Member<FixedArray<UInt64,D>,     "grid">,
	Member<Array<UInt64,1u>,         "count">,
	Member<Array<UInt64,1u>,         "slot">,
	Member<FixedArray<UInt64,1u>,    "overflow">
>;
}

/**
 * Bin a position. Clamped, so a particle that has drifted outside the domain
 * still lands in a real bin rather than indexing out of bounds.
 */
template<typename T, uint64_t D, typename PosRef, typename GridRef>
uint64_t particle_bin_of(
	const PosRef& pos,
	typename kel::lbm::native_data_type<T>::type bin_size,
	const GridRef& grid
){
	uint64_t flat = 0u;
	uint64_t stride = 1u;

	for(uint64_t i{0u}; i < D; ++i){
		auto g = grid.at({i}).get();
		auto x = pos.at({{i}}).get() / bin_size;

		int64_t b = static_cast<int64_t>(x);
		if(b < 0){
			b = 0;
		}
		if(static_cast<uint64_t>(b) >= g){
			b = static_cast<int64_t>(g) - 1;
		}

		flat += static_cast<uint64_t>(b) * stride;
		stride *= g;
	}

	return flat;
}

/**
 * Clears every bin. Must precede a rebuild; bins are not self-clearing.
 */
template<typename ClRef>
void clear_cell_list(ClRef&& cl){
	auto& count = cl.template get<"count">();
	const uint64_t cells = count.size().get();
	for(uint64_t i{0u}; i < cells; ++i){
		count.at({i}).set(0);
	}
	cl.template get<"overflow">().at({0u}).set(0);
}

/**
 * Registers every particle in its bin.
 *
 * Insertion runs in particle-index order, so the contents of each bin are
 * reproducible run to run -- which is what keeps the downstream per-cell sums
 * deterministic.
 *
 * Overflow is recorded rather than silently dropped: a lost particle would
 * show up as a quietly missing body in the fluid, which is close to
 * impossible to spot in a result.
 */
template<typename T, uint64_t D, uint64_t CellCapacity, typename PartsRef, typename ClRef>
void build_cell_list(const PartsRef& parts, ClRef&& cl){
	clear_cell_list(cl);

	auto bin_size = cl.template get<"bin_size">().at({0u}).at({}).get();
	auto& grid = cl.template get<"grid">();
	auto& count = cl.template get<"count">();
	auto& slot = cl.template get<"slot">();

	const uint64_t n = parts.get_dims().at({0u}).get();

	for(uint64_t p{0u}; p < n; ++p){
		auto& pos = parts.at({p}).template get<"rigid_body">().template get<"position">();
		auto bin = particle_bin_of<T,D>(pos,bin_size,grid);

		auto c = count.at({bin}).get();
		if(c >= CellCapacity){
			auto& of = cl.template get<"overflow">().at({0u});
			of.set(of.get() + 1u);
			continue;
		}

		slot.at({bin * CellCapacity + c}).set(p);
		count.at({bin}).set(c + 1u);
	}
}

/**
 * Calls func(particle_index) for every particle registered in the bin holding
 * `cell` or in any bin adjacent to it.
 *
 * The 3^D neighbourhood is what makes the bin-size rule above load bearing: a
 * smaller bin would let a particle reach past the neighbours it is searched in.
 */
template<typename T, uint64_t D, uint64_t CellCapacity, typename ClRef, typename CellRef, typename Func>
void for_each_bin_candidate(const ClRef& cl, const CellRef& cell, Func&& func){
	auto bin_size = cl.template get<"bin_size">().at({0u}).at({}).get();
	auto& grid = cl.template get<"grid">();
	auto& count = cl.template get<"count">();
	auto& slot = cl.template get<"slot">();

	int64_t base[D];
	for(uint64_t i{0u}; i < D; ++i){
		auto x = static_cast<typename kel::lbm::native_data_type<T>::type>(cell.at({i}).get());
		base[i] = static_cast<int64_t>(x / bin_size);
	}

	// 3^D neighbourhood, unrolled as a mixed-radix counter.
	uint64_t total = 1u;
	for(uint64_t i{0u}; i < D; ++i){
		total *= 3u;
	}

	for(uint64_t off{0u}; off < total; ++off){
		uint64_t rem = off;
		uint64_t flat = 0u;
		uint64_t stride = 1u;
		bool inside = true;

		for(uint64_t i{0u}; i < D; ++i){
			int64_t delta = static_cast<int64_t>(rem % 3u) - 1;
			rem /= 3u;

			int64_t b = base[i] + delta;
			auto g = static_cast<int64_t>(grid.at({i}).get());
			if(b < 0 || b >= g){
				inside = false;
				break;
			}

			flat += static_cast<uint64_t>(b) * stride;
			stride *= static_cast<uint64_t>(g);
		}

		if(!inside){
			continue;
		}

		auto c = count.at({flat}).get();
		for(uint64_t s{0u}; s < c; ++s){
			func(slot.at({flat * CellCapacity + s}).get());
		}
	}
}

/**
 * Sizes a cell list for a domain and a largest particle radius.
 */
template<typename T, uint64_t D, uint64_t CellCapacity, typename ExtentRef>
kel::lbm::data<sch::ParticleCellList<T,D,CellCapacity>> make_cell_list(
	const ExtentRef& extent,
	typename kel::lbm::native_data_type<T>::type max_radius
){
	kel::lbm::data<sch::ParticleCellList<T,D,CellCapacity>> cl;

	// At least one lattice unit, and at least the largest diameter.
	auto bin_size = max_radius * 2;
	if(!(bin_size > 1)){
		bin_size = 1;
	}
	cl.template get<"bin_size">().at({0u}).at({}).set(bin_size);

	auto& grid = cl.template get<"grid">();
	uint64_t cells = 1u;
	for(uint64_t i{0u}; i < D; ++i){
		auto e = static_cast<typename kel::lbm::native_data_type<T>::type>(extent.at({i}).get());
		uint64_t g = static_cast<uint64_t>(e / bin_size) + 1u;
		grid.at({i}).set(g);
		cells *= g;
	}

	cl.template get<"count">() = {kel::lbm::data<sch::FixedArray<sch::UInt64,1u>>{{cells}}};
	cl.template get<"slot">()  = {kel::lbm::data<sch::FixedArray<sch::UInt64,1u>>{{cells * CellCapacity}}};

	clear_cell_list(cl);

	return cl;
}

}
}
