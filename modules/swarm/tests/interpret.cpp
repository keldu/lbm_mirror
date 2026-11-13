#include <forstio/test/suite.hpp>

#include "../c++/interpret.hpp"

#include <string>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;
using namespace kel::lbm::swarm;

/**
 * The orchestrator's webgl2 branch cannot be reached by the native worker, and
 * reaching it with a real browser means a human in the loop. These cover the
 * part of it that is pure: turning a reported capability into a capacity and a
 * summary line.
 */

data<sch::WebGl2Capability> make_gl(uint64_t safe_bytes, uint64_t max_tex,
								   bool float_render = true){
	data<sch::WebGl2Capability> gl;
	gl.get<"limits">().get<"max_texture_size">().set(
		static_cast<uint32_t>(max_tex));
	gl.get<"limits">().get<"max_draw_buffers">().set(8u);
	gl.get<"budget">().get<"probed_bytes_safe">().set(safe_bytes);
	gl.get<"features">().get<"color_buffer_float">().set(float_render);
	return gl;
}

data<sch::NativeCapability> make_native(uint64_t memory, uint32_t threads,
										bool sycl = false){
	data<sch::NativeCapability> nat;
	nat.get<"threads">().set(threads);
	nat.get<"memory_bytes">().set(memory);
	nat.get<"cpu">().set(std::string{"x86_64"});
	nat.get<"has_sycl">().set(sycl);
	return nat;
}

SAW_TEST("Interpret / the reference packing is D2Q9 double buffered"){
	static_assert(reference_bytes_per_cell() == 96u,
		"Capacity figures are quoted in 96 byte D2Q9 cells");
	SAW_EXPECT(true, "static assert carries this one");
}

SAW_TEST("Interpret / a browser's capacity comes from its measured budget"){
	// 512MiB safe budget on a roomy device: memory binds, not texture size.
	const auto gl = make_gl(512ull * 1024ull * 1024ull, 16384u);
	const uint64_t cells = webgl2_capacity_cells(gl);

	SAW_EXPECT(cells == (512ull * 1024ull * 1024ull) / 96u,
		"Capacity is the budget divided by the reference cell size");
	SAW_EXPECT(cells * 96u <= 512ull * 1024ull * 1024ull,
		"and must fit the budget it came from");
}

SAW_TEST("Interpret / a small texture dimension binds before memory"){
	// Generous budget, minimum spec texture size: the texture bound wins.
	const auto gl = make_gl(4ull * 1024ull * 1024ull * 1024ull, 2048u);
	const uint64_t cells = webgl2_capacity_cells(gl);

	SAW_EXPECT(cells == 2048ull * 2048ull,
		"The texture dimension is the binding constraint here");
}

SAW_TEST("Interpret / a device that measured nothing has no capacity"){
	// What a worker reports when the probe failed outright -- the case the
	// node self test produces, and the one a browser produces on a device
	// with no float render target.
	const auto gl = make_gl(0u, 16384u, false);
	SAW_EXPECT(webgl2_capacity_cells(gl) == 0u,
		"No measured budget means no capacity, not a guess from the limits");
}

SAW_TEST("Interpret / the summary says when a device cannot render float"){
	const auto usable = make_gl(256ull * 1024ull * 1024ull, 16384u, true);
	const std::string ok_line = webgl2_summary(usable);
	SAW_EXPECT(ok_line.find("tex 16384") != std::string::npos,
		"The summary names the texture limit");
	SAW_EXPECT(ok_line.find("256.0 MiB") != std::string::npos,
		"and the budget, in units a human reads");
	SAW_EXPECT(ok_line.find("NO FLOAT RENDER") == std::string::npos,
		"A usable device is not flagged");

	const auto broken = make_gl(0u, 16384u, false);
	SAW_EXPECT(webgl2_summary(broken).find("NO FLOAT RENDER") != std::string::npos,
		"A device with no float render target is flagged loudly, since that "
		"is the difference between slow and impossible");
}

SAW_TEST("Interpret / a native worker's capacity leaves room for the machine"){
	const auto nat = make_native(32ull * 1024ull * 1024ull * 1024ull, 16u);
	const uint64_t cells = native_capacity_cells(nat);

	// A quarter of memory, because the rest of the machine exists.
	SAW_EXPECT(cells == (32ull * 1024ull * 1024ull * 1024ull / 4u) / 96u,
		"Native capacity is a quarter of memory in reference cells");
	SAW_EXPECT(cells * 96u * 4u <= 32ull * 1024ull * 1024ull * 1024ull,
		"and stays well inside what the machine has");
}

SAW_TEST("Interpret / native and browser capacities are comparable"){
	// The point of quoting both in the same unit: a registry holding a mix of
	// kinds can rank them against each other.
	const auto gl = make_gl(2ull * 1024ull * 1024ull * 1024ull, 16384u);
	const auto nat = make_native(8ull * 1024ull * 1024ull * 1024ull, 8u);

	const uint64_t browser_cells = webgl2_capacity_cells(gl);
	const uint64_t native_cells = native_capacity_cells(nat);

	// 2GiB measured vs 8GiB/4 = 2GiB planned: the same, which is the check
	// that one unit really is being used for both.
	SAW_EXPECT(browser_cells == native_cells,
		"Equivalent budgets must produce equal capacities across kinds");
}

SAW_TEST("Interpret / the native summary reports what a process can know"){
	const std::string plain = native_summary(
		make_native(8ull * 1024ull * 1024ull * 1024ull, 4u));
	SAW_EXPECT(plain.find("4 threads") != std::string::npos, "thread count");
	SAW_EXPECT(plain.find("8.0 GiB") != std::string::npos, "memory");
	SAW_EXPECT(plain.find("x86_64") != std::string::npos, "architecture");
	SAW_EXPECT(plain.find("sycl") == std::string::npos,
		"and does not claim sycl when there is none");

	const std::string with_sycl = native_summary(
		make_native(8ull * 1024ull * 1024ull * 1024ull, 4u, true));
	SAW_EXPECT(with_sycl.find("sycl") != std::string::npos,
		"but says so when there is");
}
}
