#pragma once

#include <cstdint>

/**
 * How much lattice fits on a worker.
 *
 * This is pure arithmetic over a descriptor's Q and a device's reported limits,
 * with no GL and no schema, which is the point: the orchestrator needs it to
 * size assignments, the worker needs it to drive its allocation trial, and
 * neither reason is a good one to make it untestable without a browser.
 *
 * Everything is constexpr so the ceilings can be asserted at compile time
 * against a descriptor rather than discovered on a device that happens to be
 * minimum spec.
 */
namespace kel {
namespace lbm {
namespace swarm {

/**
 * A WebGL2 implementation is only *guaranteed* these. Real desktop values are
 * far higher, but a plan that relies on more than this is a plan that works on
 * the developer's machine.
 */
struct webgl2_guaranteed {
	static constexpr uint32_t max_texture_size = 2048u;
	static constexpr uint32_t max_3d_texture_size = 256u;
	static constexpr uint32_t max_array_texture_layers = 256u;
	static constexpr uint32_t max_draw_buffers = 4u;
	static constexpr uint32_t max_color_attachments = 4u;
	static constexpr uint32_t max_texture_image_units = 16u;
};

/**
 * Packing a Q-direction lattice into RGBA textures.
 *
 * One texel holds four scalars, so a descriptor needs ceil(Q/4) textures and
 * wastes whatever is left over. The leftovers are not merely waste: the aux
 * fields (the per cell info flag, later the particle fields) can be stuffed
 * into them, which is why idle_slots is reported rather than just computed.
 */
struct packing_math {
	static constexpr uint64_t slots_per_texel = 4u;

	static constexpr uint64_t planes(uint64_t q){
		return (q + slots_per_texel - 1u) / slots_per_texel;
	}

	static constexpr uint64_t idle_slots(uint64_t q){
		return planes(q) * slots_per_texel - q;
	}

	/**
	 * Aux planes needed once the idle slots have been used. Stuffing only
	 * works when the leftovers are big enough; D2Q9 has three idle slots, so
	 * a single info flag fits but the four fields a PSM run wants do not.
	 */
	static constexpr uint64_t aux_planes(uint64_t q, uint64_t aux_scalars,
										 bool stuffed){
		if(stuffed and aux_scalars <= idle_slots(q)){
			return 0u;
		}
		return (aux_scalars + slots_per_texel - 1u) / slots_per_texel;
	}

	static constexpr uint64_t bytes_per_texel(uint64_t storage_bits){
		return slots_per_texel * storage_bits / 8u;
	}

	/**
	 * Distributions are double buffered for the ping pong; aux fields are not,
	 * since nothing writes them during a step.
	 */
	static constexpr uint64_t bytes_per_cell(uint64_t q, uint64_t states,
											 uint64_t aux_scalars, bool stuffed,
											 uint64_t storage_bits = 32u){
		const uint64_t tex = states * planes(q) + aux_planes(q, aux_scalars, stuffed);
		return tex * bytes_per_texel(storage_bits);
	}

	/**
	 * Render targets needed per pass, against what the device allows.
	 *
	 * This is the limit that actually decides whether a descriptor is usable.
	 * Collision couples every direction, so a multi-pass split has to re-read
	 * all planes and recompute rho and u in each pass -- roughly double the
	 * arithmetic for the same bandwidth. Worth detecting rather than
	 * stumbling into.
	 */
	static constexpr uint64_t passes(uint64_t q, uint64_t mrt){
		if(mrt == 0u){
			return 0u;
		}
		return (planes(q) + mrt - 1u) / mrt;
	}

	static constexpr bool fits_single_pass(uint64_t q, uint64_t mrt){
		return passes(q, mrt) == 1u;
	}
};

/**
 * Integer square root by bisection.
 *
 * std::sqrt is not constexpr before C++26 and this has to run at compile time
 * for the ceiling assertions. Exactness also matters more than speed here: a
 * domain edge that rounds up by one costs a whole row of cells against a
 * budget that was measured, not guessed.
 */
constexpr uint64_t isqrt(uint64_t value){
	if(value < 2u){
		return value;
	}

	uint64_t low = 1u;
	uint64_t high = (value / 2u) + 1u;

	while(low < high){
		const uint64_t mid = low + (high - low + 1u) / 2u;
		if(mid <= value / mid){
			low = mid;
		} else {
			high = mid - 1u;
		}
	}

	return low;
}

/**
 * A 2D domain and what it costs.
 */
struct domain_2d {
	uint64_t nx;
	uint64_t ny;

	constexpr uint64_t cells() const { return nx * ny; }
};

struct capacity_math {
	/**
	 * Cells the texture dimension allows, independent of memory.
	 *
	 * On a typical desktop this is around a hundred times looser than what
	 * memory actually permits, which is exactly why the worker measures
	 * instead of trusting it -- but it is still a hard ceiling and a worker
	 * that hits *this* one is telling you something different about itself
	 * than a worker that ran out of memory.
	 */
	static constexpr uint64_t texture_bound_cells_2d(uint64_t max_texture_size,
													 uint64_t ghost){
		const uint64_t pad = 2u * ghost;
		if(max_texture_size <= pad){
			return 0u;
		}
		const uint64_t edge = max_texture_size - pad;
		return edge * edge;
	}

	static constexpr uint64_t cells_from_bytes(uint64_t bytes,
											   uint64_t bytes_per_cell){
		if(bytes_per_cell == 0u){
			return 0u;
		}
		return bytes / bytes_per_cell;
	}

	static constexpr uint64_t max_cells_2d(uint64_t budget_bytes,
										   uint64_t bytes_per_cell,
										   uint64_t max_texture_size,
										   uint64_t ghost){
		const uint64_t by_memory = cells_from_bytes(budget_bytes, bytes_per_cell);
		const uint64_t by_texture = texture_bound_cells_2d(max_texture_size, ghost);
		return (by_memory < by_texture) ? by_memory : by_texture;
	}

	/**
	 * Largest domain of a given aspect ratio that fits in a cell count.
	 *
	 * Aspect arrives as a ratio of integers rather than a float so the result
	 * is reproducible across the orchestrator and the worker; they must agree
	 * exactly or an assignment will not fit the allocation the worker made.
	 *
	 * Rounds down on both axes, and clamps each edge to what the texture
	 * dimension allows, so the answer is always feasible rather than merely
	 * close.
	 */
	static constexpr domain_2d fit_domain_2d(uint64_t cells,
											 uint64_t aspect_num,
											 uint64_t aspect_den,
											 uint64_t max_texture_size,
											 uint64_t ghost){
		if(cells == 0u or aspect_num == 0u or aspect_den == 0u){
			return domain_2d{0u, 0u};
		}

		// ny = sqrt(cells * den / num), nx = ny * num / den
		uint64_t ny = isqrt((cells * aspect_den) / aspect_num);
		if(ny == 0u){
			ny = 1u;
		}
		uint64_t nx = (ny * aspect_num) / aspect_den;
		if(nx == 0u){
			nx = 1u;
		}

		const uint64_t pad = 2u * ghost;
		const uint64_t edge = (max_texture_size > pad) ? (max_texture_size - pad) : 0u;
		if(edge == 0u){
			return domain_2d{0u, 0u};
		}
		if(nx > edge){
			nx = edge;
		}
		if(ny > edge){
			ny = edge;
		}

		// Rounding up through the aspect ratio can overshoot the budget by a
		// row; give the row back rather than hand out a domain that does not
		// fit the measurement.
		while(nx * ny > cells and ny > 1u){
			--ny;
		}
		while(nx * ny > cells and nx > 1u){
			--nx;
		}

		return domain_2d{nx, ny};
	}
};

} // namespace swarm
} // namespace lbm
} // namespace kel
