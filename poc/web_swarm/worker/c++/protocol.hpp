#pragma once

#include <kel/lbm/webgl/probe.hpp>

#include <kel/lbm/swarm/message.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <cstdint>
#include <string>

/**
 * What the browser worker does, separated from how JavaScript calls it.
 *
 * worker.cpp is a thin extern "C" shim over this; keeping the behaviour in a
 * header means the self test can drive it directly under node, without a
 * browser and without going through ccall. The GL probe is the only part that
 * genuinely needs a browser, and it fails cleanly when there is no context --
 * which is exactly the state node is in, so everything else stays exercisable.
 */
namespace kel {
namespace lbm {
namespace swarm {

class worker_protocol {
public:
	struct config {
		std::string worker_id = "browser";
		std::string user_agent = "unknown";
	};

	/**
	 * Runs the capability probe.
	 *
	 * Returns false when this device cannot run a solver. That is not a reason
	 * to stay silent: the worker still registers, so the orchestrator learns
	 * the device exists and why it is unusable, rather than never hearing from
	 * it at all.
	 */
	bool init(config cfg, uint64_t now_ms){
		cfg_ = std::move(cfg);
		started_at_ms_ = now_ms;

		webgl::probe_config pcfg;
		probe_ok_ = webgl::probe_webgl2(pcfg, capability_);

		return probe_ok_;
	}

	/// The hello to send once the control channel opens.
	std::string hello(uint64_t now_ms) const {
		auto message = make_message<sch::MsgHelloWebGl2>(
			msg::hello, cfg_.worker_id, 0u);

		auto &cap = message.get<"capability">();
		cap.get<"worker_id">().set(cfg_.worker_id);
		cap.get<"kind">().set(std::string{wkind::webgl2});
		cap.get<"schema_version">().set(std::string{schema_version});
		cap.get<"reported_at_ms">().set(now_ms);
		cap.get<"user_agent">().set(cfg_.user_agent);
		cap.get<"webgl2">() = capability_;

		auto eo = encode_message<sch::MsgHelloWebGl2>(message);
		if(eo.is_error()){
			return {};
		}
		return eo.get_value();
	}

	/**
	 * Handles one control message, returning the reply or an empty string.
	 *
	 * The transport never has to know that a ping wants a pong -- only that a
	 * non-empty return goes back down the channel.
	 */
	std::string handle(const std::string &text, uint64_t now_ms){
		auto eo_hdr = peek_header(text);
		if(eo_hdr.is_error()){
			return {};
		}

		const std::string type = eo_hdr.get_value().get<"type">().stl_string();

		if(type == msg::ping){
			return handle_ping(text, now_ms);
		}
		if(type == msg::welcome){
			return handle_welcome(now_ms);
		}

		// Anything else is not ours to answer.
		return {};
	}

	uint64_t pongs() const { return pongs_; }
	bool probe_ok() const { return probe_ok_; }

	const data<sch::WebGl2Capability> &capability() const { return capability_; }

private:
	data<sch::WorkerLive> make_live(uint64_t now_ms) const {
		data<sch::WorkerLive> live;
		live.get<"state">().set(std::string{
			probe_ok_ ? wstate::ready : wstate::error});
		live.get<"uptime_ms">().set(
			(now_ms > started_at_ms_) ? (now_ms - started_at_ms_) : 0u);
		// Nothing is being simulated yet; the field exists so the monitor does
		// not need a protocol change once something is.
		live.get<"steps_done">().set(0u);
		live.get<"last_error">().set(std::string{""});
		return live;
	}

	std::string handle_ping(const std::string &text, uint64_t now_ms){
		auto eo = decode_message<sch::MsgPing>(text);
		if(eo.is_error()){
			return {};
		}

		auto pong = make_message<sch::MsgPong>(
			msg::pong, cfg_.worker_id, eo.get_value().get<"seq">().get());

		// Echo the orchestrator's own timestamp so it can measure the round
		// trip against its clock. The two machines never have to agree on what
		// time it is.
		pong.get<"sent_at_ms">().set(eo.get_value().get<"sent_at_ms">().get());
		pong.get<"worker_now_ms">().set(now_ms);
		pong.get<"live">() = make_live(now_ms);

		auto eo_out = encode_message<sch::MsgPong>(pong);
		if(eo_out.is_error()){
			return {};
		}

		++pongs_;
		return eo_out.get_value();
	}

	std::string handle_welcome(uint64_t now_ms){
		// Registration accepted. Announce readiness, which is a claim only the
		// worker can make -- the orchestrator will not assume it.
		auto ready = make_message<sch::MsgReady>(msg::ready, cfg_.worker_id, 0u);
		ready.get<"live">() = make_live(now_ms);

		auto eo = encode_message<sch::MsgReady>(ready);
		if(eo.is_error()){
			return {};
		}
		return eo.get_value();
	}

	config cfg_;
	data<sch::WebGl2Capability> capability_;
	bool probe_ok_ = false;
	uint64_t started_at_ms_ = 0u;
	uint64_t pongs_ = 0u;
};

} // namespace swarm
} // namespace lbm
} // namespace kel
