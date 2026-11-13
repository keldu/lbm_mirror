#include "protocol.hpp"

#include <emscripten.h>

#include <string>

/**
 * The browser worker, wasm half: a thin shim over worker_protocol.
 *
 * The division of labour with worker.js is deliberate and worth stating,
 * because the obvious alternative is tempting and wrong. JS owns the
 * transport: the WebSocket, the RTCPeerConnection, the data channel and the
 * timer. It moves strings and nothing else. Everything that decides what a
 * string *means* is on this side.
 *
 * The reason is single sourcing. The wire format is defined once, as schemas
 * in modules/swarm, and both the orchestrator and this worker encode and
 * decode through the same codec. A pure JS worker would have been quicker to
 * write and would have put a second, hand maintained copy of the format in a
 * language where nothing checks it against the first.
 *
 * Every entry point takes and returns plain C strings. Returned pointers point
 * into a static buffer owned here and stay valid until the next call that
 * returns a string, which is all JS needs since it copies immediately via
 * UTF8ToString.
 */
namespace {

kel::lbm::swarm::worker_protocol g_worker;

/// Backing store for whatever was last handed to JS.
std::string g_out;

const char *hand_over(std::string text){
	g_out = std::move(text);
	return g_out.c_str();
}

/**
 * Monotonic milliseconds since the page started. Deliberately performance.now
 * rather than wall clock: the live block reports an uptime, and round trips
 * are measured by echoing the orchestrator's own stamp, so nothing here needs
 * to agree with another machine's idea of the time.
 */
EM_JS(double, js_now_ms, (), {
	return performance.now();
});

uint64_t now_ms(){
	return static_cast<uint64_t>(js_now_ms());
}

} // namespace

extern "C" {

/// Runs the capability probe. Call once, before connecting. 1 if this device
/// can actually run a solver, 0 otherwise.
EMSCRIPTEN_KEEPALIVE
int kel_worker_init(const char *worker_id, const char *user_agent){
	kel::lbm::swarm::worker_protocol::config cfg;
	if(worker_id != nullptr and *worker_id != '\0'){
		cfg.worker_id = worker_id;
	}
	if(user_agent != nullptr){
		cfg.user_agent = user_agent;
	}

	return g_worker.init(std::move(cfg), now_ms()) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char *kel_worker_hello(){
	return hand_over(g_worker.hello(now_ms()));
}

EMSCRIPTEN_KEEPALIVE
const char *kel_worker_handle(const char *text){
	if(text == nullptr){
		return hand_over(std::string{});
	}
	return hand_over(g_worker.handle(std::string{text}, now_ms()));
}

EMSCRIPTEN_KEEPALIVE
double kel_worker_pongs(){
	return static_cast<double>(g_worker.pongs());
}

EMSCRIPTEN_KEEPALIVE
double kel_worker_budget_bytes(){
	return static_cast<double>(
		g_worker.capability().get<"budget">().get<"probed_bytes_safe">().get());
}

} // extern "C"

int main(){
	// Nothing to do. The page drives everything through the exports above;
	// main exists because emscripten wants an entry point.
	return 0;
}
