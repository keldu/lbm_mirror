#include "../c++/protocol.hpp"

#include <kel/lbm/swarm/message.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <cstdio>
#include <string>

/**
 * Node-hosted self test for the browser worker's protocol half.
 *
 * A check phase has no browser, so the real worker cannot be tested there.
 * But only the GL probe actually needs one -- the part that decides what to
 * say, and says it in the format the orchestrator parses, is ordinary code.
 * Built for node and run in the derivation's check phase, this covers that
 * part and leaves only the probe genuinely untested.
 *
 * Running with no WebGL context is not a limitation here, it is the point: it
 * is the same path a browser takes when the device cannot give one, and the
 * worker has to register anyway so the orchestrator learns why.
 */
namespace {

using namespace kel::lbm;

int failures = 0;

void check(bool ok, const char *what){
	if(ok){
		std::printf("  ok   %s\n", what);
		return;
	}
	std::printf("  FAIL %s\n", what);
	++failures;
}

} // namespace

int main(){
	std::printf("worker protocol self test\n");

	swarm::worker_protocol worker;

	swarm::worker_protocol::config cfg;
	cfg.worker_id = "selftest";
	cfg.user_agent = "node";

	// No WebGL under node, so the probe must fail -- cleanly, without taking
	// the rest of the worker down with it.
	const bool usable = worker.init(cfg, 1000u);
	check(not usable, "probe reports the device unusable without a context");
	check(not worker.probe_ok(), "and remembers that it did");

	// Registration still happens. An orchestrator that knows why a device is
	// unusable is better off than one that never heard from it.
	const std::string hello = worker.hello(2000u);
	check(not hello.empty(), "an unusable device still produces a hello");

	{
		auto eo = swarm::decode_message<sch::MsgHelloWebGl2>(hello);
		check(not eo.is_error(), "the hello decodes as a webgl2 hello");
		if(not eo.is_error()){
			const auto &cap = eo.get_value().get<"capability">();
			check(cap.get<"kind">().stl_view() == wkind::webgl2,
				  "it registers as a webgl2 worker");
			check(cap.get<"worker_id">().stl_view() == "selftest",
				  "it carries the configured id");
			check(cap.get<"schema_version">().stl_view() == schema_version,
				  "it states the schema version the orchestrator checks");
			check(not cap.get<"webgl2">().get<"features">()
					.get<"color_buffer_float">().get(),
				  "and honestly reports no float render target");
		}
	}

	// The routing decode the orchestrator does first has to work on it too.
	{
		auto eo = swarm::decode_message<sch::MsgHello>(hello);
		check(not eo.is_error(), "the orchestrator's routing decode accepts it");
	}

	// A ping must come back as a pong echoing the sequence and the timestamp.
	{
		auto ping = swarm::make_message<sch::MsgPing>(msg::ping, "orch", 42u);
		ping.get<"sent_at_ms">().set(123456u);

		auto eo_ping = swarm::encode_message<sch::MsgPing>(ping);
		check(not eo_ping.is_error(), "a ping encodes");

		const std::string reply = worker.handle(eo_ping.get_value(), 3000u);
		check(not reply.empty(), "a ping is answered");

		auto eo_pong = swarm::decode_message<sch::MsgPong>(reply);
		check(not eo_pong.is_error(), "the answer is a well formed pong");
		if(not eo_pong.is_error()){
			const auto &pong = eo_pong.get_value();
			check(pong.get<"type">().stl_view() == msg::pong, "typed as a pong");
			check(pong.get<"seq">().get() == 42u,
				  "echoing the sequence, so a late pong can be told apart");
			check(pong.get<"sent_at_ms">().get() == 123456u,
				  "echoing the timestamp, so rtt needs no clock agreement");
			check(pong.get<"worker_now_ms">().get() == 3000u,
				  "and carrying the worker's own clock");
			check(pong.get<"live">().get<"state">().stl_view() == wstate::error,
				  "reporting error state, since the probe failed");
		}

		check(worker.pongs() == 1u, "the pong is counted");
	}

	// A welcome must produce a ready. Readiness is a claim only the worker
	// makes; the orchestrator never assumes it.
	{
		auto welcome = swarm::make_message<sch::MsgWelcome>(
			msg::welcome, "w1", 0u);
		welcome.get<"heartbeat_ms">().set(2000u);
		welcome.get<"schema_version">().set(std::string{schema_version});

		auto eo_wel = swarm::encode_message<sch::MsgWelcome>(welcome);
		check(not eo_wel.is_error(), "a welcome encodes");

		const std::string reply = worker.handle(eo_wel.get_value(), 4000u);
		check(not reply.empty(), "a welcome is answered");

		auto eo_ready = swarm::decode_message<sch::MsgReady>(reply);
		check(not eo_ready.is_error(), "the answer is a well formed ready");
		if(not eo_ready.is_error()){
			check(eo_ready.get_value().get<"type">().stl_view() == msg::ready,
				  "typed as ready");
		}
	}

	// Things the worker has no business answering must produce nothing rather
	// than an empty message on the wire.
	{
		check(worker.handle("", 5000u).empty(), "garbage is not answered");
		check(worker.handle("{", 5000u).empty(), "malformed json is not answered");
		check(worker.handle(R"({"type":"pong","worker_id":"x","seq":1})", 5000u)
				  .empty(),
			  "a message meant for the orchestrator is not answered");
		check(worker.pongs() == 1u, "and none of that counted as a pong");
	}

	std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
