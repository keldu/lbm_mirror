/**
 * The webgl module's smoke target.
 *
 * Nothing here can be meaningfully run without a browser, so its job is to make
 * the compiler and linker check what a check phase cannot:
 *
 *   1. em++ finds the kel-lbm core and swarm headers. It is not a nix
 *      cc-wrapper, so it never reads NIX_CFLAGS_COMPILE and cannot pick up
 *      buildInputs; the include roots arrive through the kel_include scons
 *      variable instead.
 *   2. The core headers are clean enough for a wasm target. descriptor.hpp is
 *      the narrow include the worker uses -- deliberately not
 *      <kel/lbm/lbm.hpp>, which drags in util.hpp's <sys/ioctl.h>, the vtk and
 *      csv writers and <iostream>.
 *   3. Every GL and emscripten entry point the probe calls actually resolves.
 *      Emscripten sets ERROR_ON_UNDEFINED_SYMBOLS by default, so linking this
 *      is a real check on probe.cpp rather than a formality.
 *   4. The chain the browser worker depends on composes end to end: probe a
 *      capability, put it in a hello, encode it to the text that goes on the
 *      data channel.
 *   5. Both the .js loader and the .wasm sidecar are produced.
 *
 * The descriptor tables are consumed through a static_assert rather than merely
 * included, so the compiler is forced to instantiate df_info and this cannot
 * silently degrade into "the header parsed".
 */

#include "probe.hpp"

#include <kel/lbm/descriptor.hpp>
#include <kel/lbm/swarm/message.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
// Qualified rather than aliased: a local `sch` would collide with kel::lbm::sch
// the moment main pulls kel::lbm into scope.
using Desc = kel::lbm::sch::D2Q9;
using dfi = kel::lbm::df_info<kel::lbm::sch::Float32, Desc>;

static_assert(Desc::D == 2u, "D2Q9 has to be two dimensional");
static_assert(Desc::Q == 9u, "D2Q9 has to carry nine directions");

/**
 * Opposites have to pair up. This is the cheapest property that actually
 * touches the constexpr tables, which is the point -- it proves instantiation
 * rather than parsing.
 */
constexpr bool opposites_pair(){
	for(uint64_t i = 0u; i < Desc::Q; ++i){
		if(dfi::opposite_index[dfi::opposite_index[i]] != i){
			return false;
		}
	}
	return true;
}

static_assert(opposites_pair(), "opposite_index has to be an involution");
}

int main(){
	using namespace kel::lbm;

	webgl::probe_config cfg;

	// Without a canvas this returns false immediately, which is the correct
	// behaviour and is all that can be checked outside a browser. The call is
	// here so the signatures and the capability plumbing are compiled against.
	data<sch::WebGl2Capability> gl{};
	const bool usable = webgl::probe_webgl2(cfg, gl);

	// The chain a real worker walks: capability into a hello, hello onto the
	// wire. If this compiles and links, step six has no surprises waiting.
	auto hello = swarm::make_message<sch::MsgHelloWebGl2>(
		msg::hello, "probe", 0u);

	auto &cap = hello.get<"capability">();
	cap.get<"worker_id">().set(std::string{"probe"});
	cap.get<"kind">().set(std::string{wkind::webgl2});
	cap.get<"schema_version">().set(std::string{schema_version});
	cap.get<"reported_at_ms">().set(0u);
	cap.get<"user_agent">().set(std::string{"kel-lbm-webgl-probe"});
	cap.get<"webgl2">() = gl;

	auto eo_text = swarm::encode_message<sch::MsgHelloWebGl2>(hello);
	if(eo_text.is_error()){
		return 1;
	}

	std::printf("usable=%d hello=%zu bytes\n",
				usable ? 1 : 0, eo_text.get_value().size());

	return 0;
}
