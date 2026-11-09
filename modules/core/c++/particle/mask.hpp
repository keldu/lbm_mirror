#pragma once

#include <cmath>

#include "common.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {

/**
 * Voxel shape convention
 * ======================
 *
 * The group's mask is a D-cube of R voxels per axis holding a SIGNED DISTANCE
 * in NORMALIZED PARTICLE-LOCAL units, where the shape's bounding radius is
 * exactly 1. So the mask spans local [-1,1]^D and
 *
 *     step        = 2 / R
 *     u_local(k)  = -1 + (k + 0.5) * step        (cell centred)
 *     k(u_local)  = (u_local + 1) / step - 0.5
 *
 * Normalized rather than lattice units because radius is now a per-particle
 * property: one group mask has to serve particles of different sizes, and the
 * physical radius is then exactly `scale`. That also makes the bounding-sphere
 * broad phase agree with the voxel narrow phase by construction -- spheres
 * disjoint implies voxels disjoint -- instead of by argument.
 *
 * Local to global, and its inverse, which is what the narrow phase needs:
 *
 *     x_global = pos + scale * Rot(theta) * (u_local - com)
 *     u_local  = Rot(-theta) * (x_global - pos) / scale + com
 *
 * Signed distance rather than a 0/1 indicator because the indicator's gradient
 * is a one-cell delta quantized to lattice directions, which cannot give a
 * usable contact normal. From an SDF the normal is grad(phi) and the
 * penetration is -phi directly, and a smooth solid fraction is still one cheap
 * ramp away. Switching later would mean rebuilding every mask, so it is
 * decided here.
 *
 * Sign: negative inside the body, positive outside. Solid fraction is derived,
 * and porosity stays 1 = fluid / 0 = solid to match the analytic path.
 */

namespace impl {
/**
 * Rotates a local offset into the global frame.
 *
 * 2D rotation is an angle; 3D is a rotation vector (axis * angle) applied via
 * Rodrigues, which matches how verlet_step_lambda integrates it.
 */
template<typename T, uint64_t D, typename RotRef>
kel::lbm::data<sch::Vector<T,D>> mask_rotate(
	const kel::lbm::data<sch::Vector<T,D>>& v,
	const RotRef& rot,
	bool inverse
){
	if constexpr ( D == 2u ){
		auto a = rot.at({}).get();
		if(inverse){
			a = -a;
		}
		auto c = std::cos(a);
		auto s = std::sin(a);

		kel::lbm::data<sch::Vector<T,D>> out;
		out.at({{0u}}).set(v.at({{0u}}).get() * c - v.at({{1u}}).get() * s);
		out.at({{1u}}).set(v.at({{0u}}).get() * s + v.at({{1u}}).get() * c);
		return out;
	}else{
		kel::lbm::data<sch::Vector<T,3u>> r;
		for(uint64_t i{0u}; i < 3u; ++i){
			r.at({{i}}).set(rot.at({{i}}).get());
		}

		auto theta_2 = kel::lbm::math::dot(r,r).at({}).get();
		if(!(theta_2 > 0)){
			return v;
		}
		auto theta = std::sqrt(theta_2);
		if(inverse){
			theta = -theta;
		}

		kel::lbm::data<sch::Vector<T,3u>> n;
		for(uint64_t i{0u}; i < 3u; ++i){
			n.at({{i}}).set(r.at({{i}}).get() / std::sqrt(theta_2));
		}

		auto c = std::cos(theta);
		auto s = std::sin(theta);
		auto n_dot_v = kel::lbm::math::dot(n,v).at({}).get();
		auto cross = kel::lbm::math::cross<T>(n,v);

		kel::lbm::data<sch::Vector<T,D>> out;
		for(uint64_t i{0u}; i < D; ++i){
			out.at({{i}}).set(
				v.at({{i}}).get() * c
				+ cross.at({{i}}).get() * s
				+ n.at({{i}}).get() * n_dot_v * (1 - c)
			);
		}
		return out;
	}
}
}

/**
 * Voxel index -> normalized local position (cell centred).
 */
template<typename T, uint64_t D, typename IndexRef>
kel::lbm::data<sch::Vector<T,D>> mask_voxel_to_local(
	const IndexRef& k,
	uint64_t resolution
){
	auto step = typename kel::lbm::native_data_type<T>::type(2)
		/ static_cast<typename kel::lbm::native_data_type<T>::type>(resolution);

	kel::lbm::data<sch::Vector<T,D>> u;
	for(uint64_t i{0u}; i < D; ++i){
		auto ki = static_cast<typename kel::lbm::native_data_type<T>::type>(k.at({i}).get());
		u.at({{i}}).set(-1 + (ki + 0.5) * step);
	}
	return u;
}

/**
 * Normalized local -> global lattice position.
 */
template<typename T, uint64_t D, typename PRef, typename ComRef>
kel::lbm::data<sch::Vector<T,D>> mask_local_to_global(
	const kel::lbm::data<sch::Vector<T,D>>& u,
	const PRef& p,
	const ComRef& com
){
	auto& rb = p.template get<"rigid_body">();
	auto& pos = rb.template get<"position">();
	auto scale = p.template get<"collision">().template get<"radius">().at({}).get();

	kel::lbm::data<sch::Vector<T,D>> rel;
	for(uint64_t i{0u}; i < D; ++i){
		rel.at({{i}}).set(u.at({{i}}).get() - com.at({{i}}).get());
	}

	auto rot = impl::mask_rotate<T,D>(rel, rb.template get<"rotation">(), false);

	kel::lbm::data<sch::Vector<T,D>> x;
	for(uint64_t i{0u}; i < D; ++i){
		x.at({{i}}).set(pos.at({{i}}).get() + scale * rot.at({{i}}).get());
	}
	return x;
}

/**
 * Global lattice position -> normalized local. The inverse of the above, and
 * what the narrow phase and the porosity gather both go through.
 */
template<typename T, uint64_t D, typename PRef, typename ComRef>
kel::lbm::data<sch::Vector<T,D>> mask_global_to_local(
	const kel::lbm::data<sch::Vector<T,D>>& x,
	const PRef& p,
	const ComRef& com
){
	auto& rb = p.template get<"rigid_body">();
	auto& pos = rb.template get<"position">();
	auto scale = p.template get<"collision">().template get<"radius">().at({}).get();

	kel::lbm::data<sch::Vector<T,D>> rel;
	for(uint64_t i{0u}; i < D; ++i){
		rel.at({{i}}).set((x.at({{i}}).get() - pos.at({{i}}).get()) / scale);
	}

	auto unrot = impl::mask_rotate<T,D>(rel, rb.template get<"rotation">(), true);

	kel::lbm::data<sch::Vector<T,D>> u;
	for(uint64_t i{0u}; i < D; ++i){
		u.at({{i}}).set(unrot.at({{i}}).get() + com.at({{i}}).get());
	}
	return u;
}

/**
 * Samples the stored signed distance at a normalized local position, with
 * n-linear interpolation.
 *
 * Interpolated rather than nearest so the field stays continuous as a particle
 * slides across the lattice; a nearest lookup aliases whenever the voxel pitch
 * does not divide the lattice, and leaves grad(phi) piecewise constant, which
 * the contact normal cannot use.
 *
 * Outside the mask box the distance is extrapolated from the box corner, which
 * is monotone and good enough -- nothing in contact range is ever out there.
 */
template<typename T, uint64_t D, typename MaskRef>
typename kel::lbm::native_data_type<T>::type mask_sample_local(
	const MaskRef& mask,
	const kel::lbm::data<sch::Vector<T,D>>& u,
	uint64_t resolution
){
	using NT = typename kel::lbm::native_data_type<T>::type;

	auto step = NT(2) / static_cast<NT>(resolution);

	NT frac[D];
	int64_t base[D];
	NT outside = 0;

	for(uint64_t i{0u}; i < D; ++i){
		auto g = (u.at({{i}}).get() + 1) / step - NT(0.5);

		// Track how far the query sits beyond the stored box.
		if(g < 0){
			outside += g * g * step * step;
			g = 0;
		}else if(g > static_cast<NT>(resolution - 1u)){
			auto over = g - static_cast<NT>(resolution - 1u);
			outside += over * over * step * step;
			g = static_cast<NT>(resolution - 1u);
		}

		auto f = std::floor(g);
		base[i] = static_cast<int64_t>(f);
		if(base[i] > static_cast<int64_t>(resolution) - 2){
			base[i] = static_cast<int64_t>(resolution) - 2;
			if(base[i] < 0){
				base[i] = 0;
			}
		}
		frac[i] = g - static_cast<NT>(base[i]);
	}

	// n-linear blend over the 2^D surrounding voxels.
	uint64_t corners = 1u;
	for(uint64_t i{0u}; i < D; ++i){
		corners *= 2u;
	}

	NT acc = 0;
	for(uint64_t c{0u}; c < corners; ++c){
		NT w = 1;
		kel::lbm::data<sch::FixedArray<sch::UInt64,D>> k;

		for(uint64_t i{0u}; i < D; ++i){
			const bool hi = ((c >> i) & 1u) != 0u;
			auto idx = base[i] + (hi ? 1 : 0);
			if(idx < 0){
				idx = 0;
			}
			if(idx > static_cast<int64_t>(resolution) - 1){
				idx = static_cast<int64_t>(resolution) - 1;
			}
			k.at({i}).set(static_cast<uint64_t>(idx));
			w *= hi ? frac[i] : (1 - frac[i]);
		}

		acc += w * mask.at(k).get();
	}

	if(outside > 0){
		acc += std::sqrt(outside);
	}

	return acc;
}

/**
 * Signed distance from a global lattice position to the body, in LATTICE
 * units. Negative inside.
 */
template<typename T, uint64_t D, typename GroupRef, typename PRef>
typename kel::lbm::native_data_type<T>::type mask_signed_distance(
	const GroupRef& pg,
	const PRef& p,
	const kel::lbm::data<sch::Vector<T,D>>& x,
	uint64_t resolution
){
	auto& com = pg.template get<"center_of_mass">().at({0u});
	auto u = mask_global_to_local<T,D>(x,p,com);

	auto scale = p.template get<"collision">().template get<"radius">().at({}).get();
	return mask_sample_local<T,D>(pg.template get<"mask">(),u,resolution) * scale;
}

/**
 * Smooth solid fraction: 1 fully inside, 0 fully outside, with a cos^2 ramp of
 * width `eps` lattice units straddling the surface.
 *
 * Derived from the signed distance rather than stored, so one field serves
 * both the fluid coupling and the contact normal.
 */
template<typename T>
typename kel::lbm::native_data_type<T>::type solid_fraction_from_sdf(
	typename kel::lbm::native_data_type<T>::type sdf,
	typename kel::lbm::native_data_type<T>::type eps
){
	using NT = typename kel::lbm::native_data_type<T>::type;

	auto half = eps / 2;
	if(sdf <= -half){
		return 1;
	}
	if(sdf >= half){
		return 0;
	}

	// t runs 0 at the inner edge to 1 at the outer; cos^2 gives a smooth
	// 1 -> 0 with zero slope at both ends.
	auto t = (sdf + half) / eps;
	auto c = std::cos(NT(3.14159265358979323846) * t / 2);
	return c * c;
}

/**
 * Signed distance functions in normalized local units, radius 1.
 */
namespace sdf {
template<typename T, uint64_t D>
typename kel::lbm::native_data_type<T>::type sphere(
	const kel::lbm::data<sch::Vector<T,D>>& u
){
	return std::sqrt(kel::lbm::math::dot(u,u).at({}).get()) - 1;
}

/**
 * Chebyshev distance: the inscribed-box shape whose bounding radius is 1 along
 * each axis.
 */
template<typename T, uint64_t D>
typename kel::lbm::native_data_type<T>::type cube(
	const kel::lbm::data<sch::Vector<T,D>>& u
){
	typename kel::lbm::native_data_type<T>::type m = 0;
	for(uint64_t i{0u}; i < D; ++i){
		auto a = std::abs(u.at({{i}}).get());
		if(a > m){
			m = a;
		}
	}
	return m - 1;
}
}

/**
 * Fills a group's mask from a signed distance function, and returns the volume
 * centroid in normalized local units.
 *
 * The centroid is weighted by solid fraction, so it is the mass centre of the
 * voxelized body rather than the centre of its bounding box -- they differ for
 * anything asymmetric, and the physics wants the former.
 */
template<typename T, uint64_t D, typename MaskRef, typename Func>
kel::lbm::data<sch::Vector<T,D>> fill_mask_from_sdf(
	MaskRef&& mask,
	uint64_t resolution,
	Func&& sdf_fn
){
	using NT = typename kel::lbm::native_data_type<T>::type;

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> dims;
	for(uint64_t i{0u}; i < D; ++i){
		dims.at({i}).set(resolution);
	}

	kel::lbm::data<sch::Vector<T,D>> com;
	for(uint64_t i{0u}; i < D; ++i){
		com.at({{i}}).set(0);
	}

	NT weight_sum = 0;
	auto step = NT(2) / static_cast<NT>(resolution);

	iterator<D>::apply([&](const auto& k){
		auto u = mask_voxel_to_local<T,D>(k,resolution);
		auto d = sdf_fn(u);

		mask.at(k).set(d);

		// Ramp one voxel wide, purely so partially filled cells near the
		// surface weight the centroid sensibly.
		auto w = solid_fraction_from_sdf<T>(d,step);
		weight_sum += w;
		for(uint64_t i{0u}; i < D; ++i){
			com.at({{i}}).set(com.at({{i}}).get() + w * u.at({{i}}).get());
		}
	},{},dims);

	if(weight_sum > 0){
		for(uint64_t i{0u}; i < D; ++i){
			com.at({{i}}).set(com.at({{i}}).get() / weight_sum);
		}
	}

	return com;
}

}
}
