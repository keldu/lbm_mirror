#include "control.hpp"

#include <kel/lbm/swarm/interpret.hpp>
#include <kel/lbm/swarm/message.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

namespace kel {
namespace lbm {
namespace swarm {

namespace {

uint64_t now_ms(){
	using namespace std::chrono;
	return static_cast<uint64_t>(
		duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace

control::control(orchestrator &orch, control_config cfg)
	: orch_{orch}, cfg_{std::move(cfg)}, registry_{cfg_.limits} {}

control::~control(){
	stop_heartbeat();
}

void control::on_event(std::function<void(const std::string &)> handler){
	on_event_ = std::move(handler);
}

void control::emit(const std::string &line){
	if(on_event_){
		on_event_(line);
	}
}

void control::attach(){
	orch_.on_channel_open([this](const std::shared_ptr<session> &sess){
		registry_.add_session(sess->id, now_ms());
		emit("[" + sess->id + "] channel open, awaiting hello");
	});

	orch_.on_channel_closed([this](const std::shared_ptr<session> &sess){
		auto rec = registry_.remove(sess->id);
		if(rec){
			emit("[" + sess->id + "] disconnected (" +
				 (rec->worker_id.empty() ? "unregistered" : rec->worker_id) + ")");
		}
	});

	orch_.on_message([this](const std::shared_ptr<session> &sess,
							const std::string &text){
		handle_message(sess, text);
	});
}

void control::handle_message(const std::shared_ptr<session> &sess,
							 const std::string &text){
	auto eo_hdr = peek_header(text);
	if(eo_hdr.is_error()){
		emit("[" + sess->id + "] malformed control message, ignoring");
		return;
	}

	const std::string type = eo_hdr.get_value().get<"type">().stl_string();
	const uint64_t seq = eo_hdr.get_value().get<"seq">().get();

	if(type == msg::hello){
		handle_hello(sess, text);
		return;
	}

	if(type == msg::pong){
		auto eo = decode_message<sch::MsgPong>(text);
		if(eo.is_error()){
			emit("[" + sess->id + "] malformed pong, ignoring");
			return;
		}
		registry_.record_pong(sess->id, seq,
							  eo.get_value().get<"sent_at_ms">().get(),
							  eo.get_value().get<"live">(), now_ms());
		return;
	}

	if(type == msg::ready){
		auto eo = decode_message<sch::MsgReady>(text);
		if(eo.is_error()){
			emit("[" + sess->id + "] malformed ready, ignoring");
			return;
		}
		registry_.mark_ready(sess->id, eo.get_value().get<"live">(), now_ms());
		emit("[" + sess->id + "] ready");
		return;
	}

	if(type == msg::status){
		auto eo = decode_message<sch::MsgStatus>(text);
		if(eo.is_error()){
			emit("[" + sess->id + "] malformed status, ignoring");
			return;
		}
		const auto &live = eo.get_value().get<"live">();
		registry_.update_live(sess->id, live, now_ms());

		const std::string last_error = live.get<"last_error">().stl_string();
		if(not last_error.empty()){
			emit("[" + sess->id + "] reports error: " + last_error);
		}
		return;
	}

	emit("[" + sess->id + "] unknown message type '" + type + "', ignoring");
}

void control::handle_hello(const std::shared_ptr<session> &sess,
						   const std::string &text){
	// Routing pass: the common capability fields are enough to learn the kind,
	// and the backend section is skipped because it is not named here.
	auto eo_route = decode_message<sch::MsgHello>(text);
	if(eo_route.is_error()){
		emit("[" + sess->id + "] hello missing its common fields, rejecting");
		return;
	}

	const auto &common = eo_route.get_value().get<"capability">();
	const std::string kind = common.get<"kind">().stl_string();
	const std::string worker_id = common.get<"worker_id">().stl_string();
	const std::string their_version = common.get<"schema_version">().stl_string();

	if(their_version != schema_version){
		// Caught deliberately rather than suffered as a decode failure later.
		emit("[" + sess->id + "] schema version mismatch: worker says '" +
			 their_version + "', orchestrator is '" + std::string{schema_version} +
			 "' -- rejecting");
		return;
	}

	uint64_t cells = 0u;
	std::string summary;

	if(kind == wkind::webgl2){
		auto eo = decode_message<sch::MsgHelloWebGl2>(text);
		if(eo.is_error()){
			emit("[" + sess->id + "] hello claims webgl2 but has no usable "
				 "webgl2 section, rejecting");
			return;
		}
		const auto &gl = eo.get_value().get<"capability">().get<"webgl2">();
		cells = webgl2_capacity_cells(gl);
		summary = webgl2_summary(gl);

	} else if(kind == wkind::native){
		auto eo = decode_message<sch::MsgHelloNative>(text);
		if(eo.is_error()){
			emit("[" + sess->id + "] hello claims native but has no usable "
				 "native section, rejecting");
			return;
		}
		const auto &nat = eo.get_value().get<"capability">().get<"native">();
		cells = native_capacity_cells(nat);
		summary = native_summary(nat);

	} else {
		// Identifiable but unusable, which is a better outcome than a zeroed
		// registration and is exactly what the routing decode is for.
		emit("[" + sess->id + "] unsupported worker kind '" + kind +
			 "', rejecting");
		return;
	}

	const uint64_t t = now_ms();
	if(not registry_.mark_registered(sess->id, worker_id, kind, cells,
									 summary, t)){
		return;
	}

	auto welcome = make_message<sch::MsgWelcome>(msg::welcome, sess->id, 0u);
	welcome.get<"heartbeat_ms">().set(cfg_.heartbeat_ms);
	welcome.get<"schema_version">().set(std::string{schema_version});

	auto eo_out = encode_message<sch::MsgWelcome>(welcome);
	if(eo_out.is_error()){
		return;
	}
	orch_.send(sess, eo_out.get_value());

	std::ostringstream line;
	line << "[" << sess->id << "] registered " << worker_id
		 << " (" << kind << ") -- " << summary;
	emit(line.str());
}

void control::start_heartbeat(){
	if(running_.exchange(true)){
		return;
	}
	ticker_ = std::thread{[this](){ heartbeat_loop(); }};
}

void control::stop_heartbeat(){
	if(not running_.exchange(false)){
		return;
	}
	tick_cv_.notify_all();
	if(ticker_.joinable()){
		ticker_.join();
	}
}

void control::heartbeat_loop(){
	while(running_.load()){
		{
			std::unique_lock<std::mutex> lock{tick_mutex_};
			tick_cv_.wait_for(lock, std::chrono::milliseconds{cfg_.heartbeat_ms},
							  [this](){ return not running_.load(); });
		}
		if(not running_.load()){
			break;
		}

		const uint64_t t = now_ms();

		// The registry decides; this loop only carries out the decisions. In
		// particular nothing is sent while its lock is held.
		auto result = registry_.tick(t);

		for(const auto &id : result.newly_stale){
			emit("[" + id + "] stale, no response to the last " +
				 std::to_string(cfg_.limits.stale_after) + " pings");
		}

		for(const auto &rec : result.dropped){
			emit("[" + rec.session_id + "] dropped after " +
				 std::to_string(cfg_.limits.drop_after) + " missed pings (" +
				 (rec.worker_id.empty() ? "unregistered" : rec.worker_id) + ")");

			// Tear the transport down too, or the session lingers with nothing
			// watching it.
			if(auto sess = orch_.find_session(rec.session_id)){
				if(sess->dc){
					sess->dc->close();
				}
				if(sess->pc){
					sess->pc->close();
				}
			}
		}

		for(const auto &target : result.to_ping){
			auto sess = orch_.find_session(target.session_id);
			if(not sess){
				continue;
			}

			auto ping = make_message<sch::MsgPing>(msg::ping, target.session_id,
												   target.seq);
			ping.get<"sent_at_ms">().set(t);

			auto eo = encode_message<sch::MsgPing>(ping);
			if(eo.is_error()){
				continue;
			}
			orch_.send(sess, eo.get_value());
		}
	}
}

} // namespace swarm
} // namespace lbm
} // namespace kel
