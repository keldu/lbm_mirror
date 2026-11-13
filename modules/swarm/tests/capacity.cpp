#include <forstio/test/suite.hpp>

#include "../c++/capacity.hpp"

namespace {
using namespace kel::lbm::swarm;

/**
 * The packing arithmetic is small enough that the interesting thing to test is
 * not that it computes, but that the ceilings it implies are the ones we think
 * they are. These are compile time assertions on purpose: they are statements
 * about which descriptors are usable, and the answer should not depend on
 * running the test on a particular device.
 */

SAW_TEST("Packing / planes and idle slots per descriptor"){
	// D1Q3, D2Q5, D2Q9, D3Q19, D3Q27.
	static_assert(packing_math::planes(3u) == 1u, "D1Q3 needs one texture");
	static_assert(packing_math::planes(5u) == 2u, "D2Q5 needs two textures");
	static_assert(packing_math::planes(9u) == 3u, "D2Q9 needs three textures");
	static_assert(packing_math::planes(19u) == 5u, "D3Q19 needs five textures");
	static_assert(packing_math::planes(27u) == 7u, "D3Q27 needs seven textures");

	static_assert(packing_math::idle_slots(3u) == 1u, "D1Q3 wastes one slot");
	static_assert(packing_math::idle_slots(5u) == 3u, "D2Q5 wastes three slots");
	static_assert(packing_math::idle_slots(9u) == 3u, "D2Q9 wastes three slots");
	static_assert(packing_math::idle_slots(19u) == 1u, "D3Q19 wastes one slot");
	static_assert(packing_math::idle_slots(27u) == 1u, "D3Q27 wastes one slot");

	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Packing / slots are a partition of the texels"){
	// planes*4 is exactly Q plus the waste, for every Q. Cheap, but it is the
	// invariant every slot table depends on.
	for(uint64_t q = 1u; q <= 64u; ++q){
		const uint64_t total = packing_math::planes(q) * packing_math::slots_per_texel;
		SAW_EXPECT(total == q + packing_math::idle_slots(q),
			"planes*4 must be Q plus the idle slots");
		SAW_EXPECT(packing_math::idle_slots(q) < packing_math::slots_per_texel,
			"Waste below a full texel, or a plane was over allocated");
	}
}

/**
 * The limit that decides whether a descriptor is usable at all. WebGL2
 * guarantees only four colour attachments, so the 3D descriptors cannot write
 * their whole distribution in one pass on a conformant minimum spec device.
 * Asserting it here means we learn it from a test rather than from a browser.
 */
SAW_TEST("Packing / single pass only reaches Q <= 16 at guaranteed limits"){
	constexpr uint64_t mrt = webgl2_guaranteed::max_draw_buffers;

	static_assert(packing_math::fits_single_pass(9u, mrt),
		"D2Q9 must fit one pass on any conformant WebGL2");
	static_assert(packing_math::fits_single_pass(5u, mrt), "D2Q5 must fit");
	static_assert(packing_math::fits_single_pass(3u, mrt), "D1Q3 must fit");

	static_assert(not packing_math::fits_single_pass(19u, mrt),
		"D3Q19 must not claim a single pass at guaranteed limits");
	static_assert(not packing_math::fits_single_pass(27u, mrt),
		"D3Q27 must not claim a single pass at guaranteed limits");

	static_assert(packing_math::passes(19u, mrt) == 2u, "D3Q19 needs two passes");
	static_assert(packing_math::passes(27u, mrt) == 2u, "D3Q27 needs two passes");

	// Sixteen is the boundary: four planes, four attachments.
	static_assert(packing_math::fits_single_pass(16u, mrt), "Q=16 is the last that fits");
	static_assert(not packing_math::fits_single_pass(17u, mrt), "Q=17 must not fit");

	SAW_EXPECT(packing_math::passes(9u, 0u) == 0u,
		"A device reporting no draw buffers must not divide by zero");
}

SAW_TEST("Packing / stuffing aux fields into idle slots"){
	// D2Q9 has three idle slots, so a lone info flag rides along free...
	static_assert(packing_math::aux_planes(9u, 1u, true) == 0u,
		"One aux scalar must fit D2Q9's idle slots");
	// ...but the four fields a PSM run wants do not.
	static_assert(packing_math::aux_planes(9u, 4u, true) == 1u,
		"Four aux scalars must not fit three idle slots");
	// Unstuffed always pays for its own plane.
	static_assert(packing_math::aux_planes(9u, 1u, false) == 1u,
		"Unstuffed aux must take a plane of its own");

	// D3Q19 and D3Q27 have exactly one idle slot, so info is free there too.
	static_assert(packing_math::aux_planes(19u, 1u, true) == 0u, "D3Q19 stuffs info");
	static_assert(packing_math::aux_planes(27u, 1u, true) == 0u, "D3Q27 stuffs info");

	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Packing / bytes per cell"){
	// D2Q9, double buffered, info stuffed: 2*3 textures * 16 bytes.
	static_assert(packing_math::bytes_per_cell(9u, 2u, 1u, true) == 96u,
		"D2Q9 stuffed must be 96 bytes per cell");
	// Unstuffed adds a plane: 2*3 + 1 textures.
	static_assert(packing_math::bytes_per_cell(9u, 2u, 1u, false) == 112u,
		"D2Q9 unstuffed must be 112 bytes per cell");
	static_assert(packing_math::bytes_per_cell(19u, 2u, 1u, true) == 160u,
		"D3Q19 stuffed must be 160 bytes per cell");
	static_assert(packing_math::bytes_per_cell(27u, 2u, 1u, true) == 224u,
		"D3Q27 stuffed must be 224 bytes per cell");

	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Capacity / integer square root is exact"){
	static_assert(isqrt(0u) == 0u, "isqrt(0)");
	static_assert(isqrt(1u) == 1u, "isqrt(1)");
	static_assert(isqrt(2u) == 1u, "isqrt(2)");
	static_assert(isqrt(3u) == 1u, "isqrt(3)");
	static_assert(isqrt(4u) == 2u, "isqrt(4)");

	// The property, not just the samples: isqrt(n)^2 <= n < (isqrt(n)+1)^2.
	for(uint64_t n = 0u; n < 4096u; ++n){
		const uint64_t r = isqrt(n);
		SAW_EXPECT(r * r <= n, "isqrt must not overshoot");
		SAW_EXPECT((r + 1u) * (r + 1u) > n, "isqrt must not undershoot");
	}

	// Exact squares near the top of the range, where a float sqrt would drift.
	const uint64_t big = 3037000499u;
	SAW_EXPECT(isqrt(big * big) == big, "isqrt must stay exact for large squares");
	SAW_EXPECT(isqrt(big * big - 1u) == big - 1u, "isqrt must round down");
}

SAW_TEST("Capacity / the texture bound is the squared usable edge"){
	// Ghost eats a ring off each side, so the edge is S - 2G.
	static_assert(capacity_math::texture_bound_cells_2d(2048u, 0u) == 2048u * 2048u,
		"No ghost means the full edge");
	static_assert(capacity_math::texture_bound_cells_2d(2048u, 1u) == 2046u * 2046u,
		"A one cell ring costs two off the edge");

	// A device whose texture dimension cannot even hold the ring has no room.
	static_assert(capacity_math::texture_bound_cells_2d(2u, 1u) == 0u,
		"An edge consumed entirely by ghost leaves nothing");

	SAW_EXPECT(true, "static asserts carry this one");
}

/**
 * The headline number: how much lattice a worker can actually hold. The point
 * of the test is the relationship between the two bounds, since on real
 * hardware the texture bound is roughly a hundred times looser than memory and
 * a capacity model that used it would be badly wrong.
 */
SAW_TEST("Capacity / memory binds before texture size on real devices"){
	constexpr uint64_t bpc = packing_math::bytes_per_cell(9u, 2u, 1u, true);
	static_assert(bpc == 96u, "D2Q9 stuffed");

	// A typical desktop: 16384 max texture, 512MB of budget.
	constexpr uint64_t desktop_tex = 16384u;
	constexpr uint64_t budget = 512ull * 1024ull * 1024ull;

	constexpr uint64_t by_texture = capacity_math::texture_bound_cells_2d(desktop_tex, 0u);
	constexpr uint64_t by_memory = capacity_math::cells_from_bytes(budget, bpc);

	static_assert(by_memory < by_texture,
		"Memory has to be the binding constraint on a typical desktop");
	static_assert(by_texture / by_memory > 40u,
		"The texture bound should be looser by a wide margin, not a hair");

	constexpr uint64_t cells = capacity_math::max_cells_2d(budget, bpc, desktop_tex, 0u);
	static_assert(cells == by_memory, "max_cells must take the tighter bound");

	SAW_EXPECT(cells * bpc <= budget, "The answer has to fit the budget it came from");
}

SAW_TEST("Capacity / minimum spec can be texture bound instead"){
	constexpr uint64_t bpc = packing_math::bytes_per_cell(9u, 2u, 1u, true);

	// Guaranteed limits with a generous budget: now the texture dimension is
	// what runs out first, which is a different thing to report upstream.
	constexpr uint64_t tex = webgl2_guaranteed::max_texture_size;
	constexpr uint64_t budget = 4ull * 1024ull * 1024ull * 1024ull;

	constexpr uint64_t by_texture = capacity_math::texture_bound_cells_2d(tex, 1u);
	constexpr uint64_t by_memory = capacity_math::cells_from_bytes(budget, bpc);

	static_assert(by_texture < by_memory, "Texture size must bind at minimum spec");
	static_assert(capacity_math::max_cells_2d(budget, bpc, tex, 1u) == by_texture,
		"max_cells must take the tighter bound here too");

	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Capacity / a fitted domain always fits"){
	constexpr uint64_t bpc = packing_math::bytes_per_cell(9u, 2u, 1u, true);

	// Sweep budgets and aspect ratios; the invariant is that the answer never
	// exceeds the cell count it was given, and never exceeds the texture edge.
	const uint64_t aspects[][2] = {{1u,1u}, {4u,1u}, {1u,4u}, {16u,9u}, {2u,1u}};
	const uint64_t tex = 16384u;
	const uint64_t ghost = 1u;
	const uint64_t edge = tex - 2u * ghost;

	for(uint64_t mb = 1u; mb <= 2048u; mb *= 2u){
		const uint64_t budget = mb * 1024ull * 1024ull;
		const uint64_t cells = capacity_math::max_cells_2d(budget, bpc, tex, ghost);

		for(const auto& a : aspects){
			const auto dom = capacity_math::fit_domain_2d(cells, a[0], a[1], tex, ghost);

			SAW_EXPECT(dom.cells() <= cells,
				"A fitted domain must not exceed the measured capacity");
			SAW_EXPECT(dom.nx <= edge and dom.ny <= edge,
				"A fitted domain must not exceed the texture edge");
			SAW_EXPECT(dom.nx > 0u and dom.ny > 0u,
				"A non zero budget must yield a non empty domain");
			SAW_EXPECT(dom.cells() * bpc <= budget,
				"The domain must fit the byte budget it was derived from");
		}
	}
}

SAW_TEST("Capacity / fitting respects the requested aspect"){
	// 4:1 on a square-ish budget should come out roughly four times as wide as
	// it is tall. Exactness is not the claim -- flooring twice costs a little --
	// but the shape has to be recognisably what was asked for.
	const auto dom = capacity_math::fit_domain_2d(1024u * 256u, 4u, 1u, 16384u, 0u);

	SAW_EXPECT(dom.nx > dom.ny, "A 4:1 request must be wider than tall");
	const uint64_t ratio_x100 = (dom.nx * 100u) / dom.ny;
	SAW_EXPECT(ratio_x100 >= 380u and ratio_x100 <= 420u,
		"A 4:1 request must land near 4:1");

	// Degenerate inputs are answered, not asserted on.
	const auto zero = capacity_math::fit_domain_2d(0u, 4u, 1u, 16384u, 0u);
	SAW_EXPECT(zero.cells() == 0u, "No capacity means no domain");
	const auto bad = capacity_math::fit_domain_2d(1024u, 0u, 1u, 16384u, 0u);
	SAW_EXPECT(bad.cells() == 0u, "A degenerate aspect must not divide by zero");
}
}
