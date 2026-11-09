#pragma once

#include <cmath>

#include "common.hpp"
#include "schema.hpp"
#include "mask.hpp"
#include "aabb.hpp"
#include "contact.hpp"

namespace kel {
namespace lbm {

/**
 * Voxel narrow phase.
 *
 * Resolves a contact from the overlap of two bodies' signed distance fields
 * rather than from an analytic shape test, so anything that can be voxelized
 * participates -- spheres, cubes, whatever the mask holds.
 *
 * Why it iterates the LATTICE, not either particle's voxel grid
 * ------------------------------------------------------------
 * Two reasons, and the second is the important one:
 *
 *  - Cost. Sweeping one body's R^D voxels is hopeless at useful resolutions.
 *    The intersection of the two integer lattice bounding boxes is tiny for
 *    near-touching bodies, which is the only case that matters.
 *
 *  - Symmetry. Both owners of a pair evaluate it independently and neither
 *    writes the other, so the two halves must come out exactly equal and
 *    opposite or every contact leaks momentum. Sampling on a particle-local
 *    grid makes the sample points depend on WHICH particle you started from.
 *    The lattice is common to both, so with a canonical (lo,hi) ordering both
 *    work items visit identical points in identical order and the results are
 *    bitwise identical.
 */

namespace impl {
/**
 * Gradient of the signed distance at a global point, by central differences.
 *
 * Taken in global coordinates directly, so no rotation of the result is
 * needed -- the field is already expressed there.
 */
template<typename T, uint64_t D, typename GroupRef, typename PRef>
kel::lbm::data<sch::Vector<T,D>> sdf_gradient(
	const GroupRef& pg,
	const PRef& p,
	const kel::lbm::data<sch::Vector<T,D>>& x,
	uint64_t resolution,
	typename kel::lbm::native_data_type<T>::type h
){
	kel::lbm::data<sch::Vector<T,D>> g;

	for(uint64_t i{0u}; i < D; ++i){
		auto lo = x;
		auto hi = x;
		lo.at({{i}}).set(x.at({{i}}).get() - h);
		hi.at({{i}}).set(x.at({{i}}).get() + h);

		auto d_lo = mask_signed_distance<T,D>(pg,p,lo,resolution);
		auto d_hi = mask_signed_distance<T,D>(pg,p,hi,resolution);

		g.at({{i}}).set((d_hi - d_lo) / (2 * h));
	}

	return g;
}
}

/**
 * Contact between two bodies of the same group, from their voxel overlap.
 *
 * `lo` and `hi` must be passed in the caller's canonical order. `normal`
 * points from hi toward lo, matching the analytic narrow phase so the force
 * law downstream is unchanged.
 */
template<typename T, uint64_t D, typename GroupRef, typename PRef>
bool voxel_contact(
	const GroupRef& pg,
	const PRef& lo,
	const PRef& hi,
	const kel::lbm::data<sch::FixedArray<sch::UInt64,D>>& extent,
	kel::lbm::data<sch::Scalar<T>> eps,
	kel::lbm::data<sch::ContactManifold<T,D>>& out
){
	using NT = typename kel::lbm::native_data_type<T>::type;

	auto& mask = pg.template get<"mask">();
	const uint64_t resolution = mask.get_dims().at({0u}).get();

	auto& pos_lo = lo.template get<"rigid_body">().template get<"position">();
	auto& pos_hi = hi.template get<"rigid_body">().template get<"position">();
	auto r_lo = lo.template get<"collision">().template get<"radius">().at({}).get();
	auto r_hi = hi.template get<"collision">().template get<"radius">().at({}).get();

	/**
	 * Intersection of the two bounding boxes, clamped to the lattice. Built
	 * from the bounding radii, which the normalized mask guarantees are a
	 * true bound -- so a non-empty overlap can never be missed here.
	 */
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> start, stop;
	for(uint64_t i{0u}; i < D; ++i){
		auto a_lo = pos_lo.at({{i}}).get() - r_lo;
		auto a_hi = pos_hi.at({{i}}).get() - r_hi;
		auto a = (a_lo > a_hi) ? a_lo : a_hi;

		auto b_lo = pos_lo.at({{i}}).get() + r_lo;
		auto b_hi = pos_hi.at({{i}}).get() + r_hi;
		auto b = (b_lo < b_hi) ? b_lo : b_hi;

		if(a < 0){ a = 0; }
		int64_t ai = static_cast<int64_t>(std::floor(a));
		int64_t bi = static_cast<int64_t>(std::ceil(b)) + 1;

		auto e = static_cast<int64_t>(extent.at({i}).get());
		if(ai < 0){ ai = 0; }
		if(bi > e){ bi = e; }
		if(bi <= ai){
			return false;
		}

		start.at({i}).set(static_cast<uint64_t>(ai));
		stop.at({i}).set(static_cast<uint64_t>(bi));
	}

	NT volume = 0;
	kel::lbm::data<sch::Vector<T,D>> centroid;
	kel::lbm::data<sch::Vector<T,D>> grad_sum;
	for(uint64_t i{0u}; i < D; ++i){
		centroid.at({{i}}).set(0);
		grad_sum.at({{i}}).set(0);
	}

	const NT eps_v = eps.at({}).get();
	// One lattice unit is the natural differencing width here.
	const NT h = 0.5;

	iterator<D>::apply([&](const auto& cell){
		kel::lbm::data<sch::Vector<T,D>> x;
		for(uint64_t i{0u}; i < D; ++i){
			x.at({{i}}).set(static_cast<NT>(cell.at({i}).get()));
		}

		auto d_lo = mask_signed_distance<T,D>(pg,lo,x,resolution);
		auto s_lo = solid_fraction_from_sdf<T>(d_lo,eps_v);
		if(!(s_lo > 0)){
			return;
		}

		auto d_hi = mask_signed_distance<T,D>(pg,hi,x,resolution);
		auto s_hi = solid_fraction_from_sdf<T>(d_hi,eps_v);
		if(!(s_hi > 0)){
			return;
		}

		auto w = s_lo * s_hi;
		volume += w;

		for(uint64_t i{0u}; i < D; ++i){
			centroid.at({{i}}).set(centroid.at({{i}}).get() + w * x.at({{i}}).get());
		}

		/**
		 * grad(phi_hi) - grad(phi_lo), weighted by the overlap.
		 *
		 * Antisymmetric in (lo,hi) by construction, which is what lets the
		 * two owners of the pair agree exactly. A centre-to-centre normal
		 * cannot represent face-on-face contact between boxes; this can.
		 */
		auto g_lo = impl::sdf_gradient<T,D>(pg,lo,x,resolution,h);
		auto g_hi = impl::sdf_gradient<T,D>(pg,hi,x,resolution,h);
		for(uint64_t i{0u}; i < D; ++i){
			grad_sum.at({{i}}).set(
				grad_sum.at({{i}}).get() + w * (g_hi.at({{i}}).get() - g_lo.at({{i}}).get())
			);
		}
	},start,stop);

	if(!(volume > 0)){
		return false;
	}

	for(uint64_t i{0u}; i < D; ++i){
		centroid.at({{i}}).set(centroid.at({{i}}).get() / volume);
	}

	/**
	 * phi is negative inside, so grad(phi_hi) points out of hi. The mutual
	 * gradient therefore already points from hi toward lo.
	 */
	auto& n = out.template get<"normal">();
	if(!impl::safe_normalize<T,D>(grad_sum,n)){
		kel::lbm::data<sch::Vector<T,D>> fallback;
		for(uint64_t i{0u}; i < D; ++i){
			fallback.at({{i}}).set(pos_lo.at({{i}}).get() - pos_hi.at({{i}}).get());
		}
		if(!impl::safe_normalize<T,D>(fallback,n)){
			for(uint64_t i{0u}; i < D; ++i){
				n.at({{i}}).set(0);
			}
			n.at({{0u}}).set(1);
		}
	}

	/**
	 * Penetration as the extent of the overlap projected on the normal.
	 *
	 * Not volume^(1/D): that is dimensionally plausible but wrong in
	 * magnitude, because a lens of depth d between radii r has volume
	 * ~ r^((D-1)/2) * d^((D+1)/2), so the exponent does not come out right.
	 */
	NT s_min = 0;
	NT s_max = 0;
	bool first = true;

	iterator<D>::apply([&](const auto& cell){
		kel::lbm::data<sch::Vector<T,D>> x;
		for(uint64_t i{0u}; i < D; ++i){
			x.at({{i}}).set(static_cast<NT>(cell.at({i}).get()));
		}

		auto d_lo = mask_signed_distance<T,D>(pg,lo,x,resolution);
		if(!(solid_fraction_from_sdf<T>(d_lo,eps_v) > 0.5)){
			return;
		}
		auto d_hi = mask_signed_distance<T,D>(pg,hi,x,resolution);
		if(!(solid_fraction_from_sdf<T>(d_hi,eps_v) > 0.5)){
			return;
		}

		auto s = kel::lbm::math::dot(x,n).at({}).get();
		if(first){
			s_min = s;
			s_max = s;
			first = false;
		}else{
			if(s < s_min){ s_min = s; }
			if(s > s_max){ s_max = s; }
		}
	},start,stop);

	// One lattice unit so a single-cell overlap reads as a real depth, not 0.
	NT depth = first ? NT(1) : (s_max - s_min + NT(1));

	auto cap = (r_lo < r_hi) ? r_lo : r_hi;
	if(depth > cap){
		depth = cap;
	}

	out.template get<"depth">().at({}).set(depth);

	auto& point = out.template get<"point">();
	for(uint64_t i{0u}; i < D; ++i){
		point.at({{i}}).set(centroid.at({{i}}).get());
	}

	return true;
}

}
}
