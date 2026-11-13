#pragma once

#include "capability.hpp"
#include "capacity.hpp"

#include <kel/lbm/abstract/data.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

/**
 * Turning a reported capability into the one number the orchestrator needs:
 * how much lattice this worker can hold.
 *
 * Quoted in D2Q9 cells so worker kinds are directly comparable. A browser's
 * figure comes from its measured texture budget; a native worker's from
 * addressable memory. Neither is a promise -- both ignore everything else
 * happening on the machine -- but a planning figure that means the same thing
 * across kinds is what makes a heterogeneous registry useful rather than
 * decorative.
 *
 * Here rather than in the orchestrator because it is pure and therefore worth
 * testing, and because the browser worker may eventually want to reason about
 * its own capacity the same way the orchestrator does.
 */
namespace kel {
namespace lbm {
namespace swarm {

/**
 * The reference packing every capacity figure is quoted in: D2Q9, double
 * buffered, the info flag stuffed into an idle slot. 96 bytes per cell.
 */
constexpr uint64_t reference_bytes_per_cell(){
	return packing_math::bytes_per_cell(9u, 2u, 1u, true);
}

namespace impl {

inline std::string human_bytes(uint64_t bytes){
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(1);

	if(bytes >= (1ull << 30)){
		oss << (static_cast<double>(bytes) / static_cast<double>(1ull << 30))
			<< " GiB";
	} else if(bytes >= (1ull << 20)){
		oss << (static_cast<double>(bytes) / static_cast<double>(1ull << 20))
			<< " MiB";
	} else {
		oss << bytes << " B";
	}

	return oss.str();
}

} // namespace impl

inline uint64_t webgl2_capacity_cells(const data<sch::WebGl2Capability> &cap){
	const uint64_t budget = cap.get<"budget">().get<"probed_bytes_safe">().get();
	const uint64_t max_tex = cap.get<"limits">().get<"max_texture_size">().get();

	// Both bounds, tighter wins -- a worker can be limited by either, and
	// which one it is says something different about the device.
	return capacity_math::max_cells_2d(budget, reference_bytes_per_cell(),
									   max_tex, 0u);
}

inline uint64_t native_capacity_cells(const data<sch::NativeCapability> &cap){
	const uint64_t memory = cap.get<"memory_bytes">().get();

	// No texture dimension to bind against, so memory is the only limit. Take
	// a quarter of it: the rest of the machine exists.
	return capacity_math::cells_from_bytes(memory / 4u,
										   reference_bytes_per_cell());
}

inline std::string webgl2_summary(const data<sch::WebGl2Capability> &cap){
	std::ostringstream oss;

	oss << "tex " << cap.get<"limits">().get<"max_texture_size">().get()
		<< ", mrt " << cap.get<"limits">().get<"max_draw_buffers">().get()
		<< ", budget " << impl::human_bytes(
			cap.get<"budget">().get<"probed_bytes_safe">().get());

	if(not cap.get<"features">().get<"color_buffer_float">().get()){
		// Without this there is no float render target and so no solver at
		// all. Worth shouting about rather than leaving to be inferred.
		oss << " [NO FLOAT RENDER]";
	}

	return oss.str();
}

inline std::string native_summary(const data<sch::NativeCapability> &cap){
	std::ostringstream oss;

	oss << cap.get<"threads">().get() << " threads, "
		<< impl::human_bytes(cap.get<"memory_bytes">().get())
		<< ", " << cap.get<"cpu">().stl_string();

	if(cap.get<"has_sycl">().get()){
		oss << ", sycl";
	}

	return oss.str();
}

} // namespace swarm
} // namespace lbm
} // namespace kel
