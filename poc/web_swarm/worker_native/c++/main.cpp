#include "capability.hpp"

#include <rtc/rtc.hpp>

#include <kel/lbm/swarm/message.hpp>
#include <kel/lbm/swarm/signal.hpp>
#include <kel/lbm/swarm/wire.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

/**
 * A native worker.
 *
 * Two jobs, and it is worth being explicit that the second is not a lesser one:
 *
 *   1. It is a worker kind. A process can join the swarm and be handed work,
 *      and it registers as kind "native" with the capability a process can
 *      actually report -- threads and memory -- rather than impersonating a
 *      browser and claiming a texture budget it has no way to measure.
 *
 *   2. It is the only end to end coverage the orchestrator can have in CI. The
 *      browser worker needs a browser, which a nix check phase does not have,
 *      so this is what keeps the signalling, the peer connection and the
 *      control channel under test.
 *
 * The transport is the mirror image of the orchestrator's: connect the
 * signalling socket, answer the offer, wait for the channel the orchestrator
 * created, then speak the control protocol on it.
 */
namespace {

using namespace kel::lbm;

struct worker_config {
	std::string url = "ws://127.0.0.1:8080/";
	std::string worker_id = "native";
	uint64_t timeout_ms = 15000u;

	/**
	 * Exit once the hello has been answered. This is what the check script
	 * runs; left off, the worker stays connected like a real one.
	 */
	bool once = false;

	/**
	 * Exit after answering this many pings. Zero means never. Takes
	 * precedence over --once, so a check can cover registration and liveness
	 * in a single run rather than two.
	 */
	uint64_t pongs_target = 0u;
};

std::atomic<uint64_t> g_pongs_sent{0u};

std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<bool> g_done{false};
std::atomic<int> g_exit_code{1};

uint64_t now_ms(){
	using namespace std::chrono;
	return static_cast<uint64_t>(
		duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void finish(int code){
	g_exit_code.store(code);
	g_done.store(true);
	g_cv.notify_all();
}

std::string text_of(const rtc::message_variant &msg){
	if(std::holds_alternative<rtc::string>(msg)){
		return std::get<rtc::string>(msg);
	}
	return {};
}

data<sch::WorkerLive> make_live(uint64_t started_at){
	data<sch::WorkerLive> live;
	live.get<"state">().set(std::string{wstate::ready});
	live.get<"uptime_ms">().set(now_ms() - started_at);
	live.get<"steps_done">().set(0u);
	live.get<"last_error">().set(std::string{""});
	return live;
}

void print_usage(const char *argv0){
	std::cout
		<< "Usage: " << argv0 << " [options]\n"
		<< "\n"
		<< "  --url <ws url>   Orchestrator signalling socket\n"
		<< "                   (default ws://127.0.0.1:8080/).\n"
		<< "  --id <name>      Worker id to report (default 'native').\n"
		<< "  --timeout <ms>   Give up if registration has not completed\n"
		<< "                   (default 15000).\n"
		<< "  --once           Exit 0 once the hello has been answered.\n"
		<< "                   Used by the check; without it the worker\n"
		<< "                   stays connected.\n"
		<< "  --pongs <n>      Exit 0 after answering n pings. Takes\n"
		<< "                   precedence over --once, so one run can cover\n"
		<< "                   registration and liveness together.\n"
		<< "  --help           This text.\n";
}

} // namespace

int main(int argc, char **argv){
	worker_config cfg;

	for(int i = 1; i < argc; ++i){
		const std::string arg{argv[i]};

		if(arg == "--help" or arg == "-h"){
			print_usage(argv[0]);
			return 0;
		}
		if(arg == "--url" and (i + 1) < argc){
			cfg.url = argv[++i];
			continue;
		}
		if(arg == "--id" and (i + 1) < argc){
			cfg.worker_id = argv[++i];
			continue;
		}
		if(arg == "--timeout" and (i + 1) < argc){
			cfg.timeout_ms = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}
		if(arg == "--once"){
			cfg.once = true;
			continue;
		}
		if(arg == "--pongs" and (i + 1) < argc){
			cfg.pongs_target = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}

		std::cerr << "Unknown or incomplete option: " << arg << "\n\n";
		print_usage(argv[0]);
		return 2;
	}

	const uint64_t started_at = now_ms();

	auto ws = std::make_shared<rtc::WebSocket>();
	auto pc = std::make_shared<rtc::PeerConnection>();

	/**
	 * The orchestrator creates the control channel, so the worker waits for it
	 * rather than making one. Held here so the callbacks below outlive the
	 * negotiation.
	 */
	std::shared_ptr<rtc::DataChannel> control;
	std::mutex control_mutex;

	pc->onLocalDescription([ws](rtc::Description desc){
		if(not ws->isOpen()){
			return;
		}
		data<sch::SignalDescription> out;
		out.get<"type">().set(desc.typeString());
		out.get<"sdp">().set(std::string{desc});

		auto eo = swarm::encode_message<sch::SignalDescription>(out);
		if(eo.is_error()){
			return;
		}
		ws->send(eo.get_value());
	});

	pc->onLocalCandidate([ws](rtc::Candidate cand){
		if(not ws->isOpen()){
			return;
		}
		data<sch::SignalCandidate> out;
		out.get<"type">().set(std::string{sig::candidate});
		out.get<"candidate">().set(cand.candidate());
		out.get<"mid">().set(cand.mid());

		auto eo = swarm::encode_message<sch::SignalCandidate>(out);
		if(eo.is_error()){
			return;
		}
		ws->send(eo.get_value());
	});

	pc->onStateChange([](rtc::PeerConnection::State state){
		if(state == rtc::PeerConnection::State::Failed){
			std::cerr << "peer connection failed\n";
			finish(1);
		}
	});

	pc->onDataChannel([&, started_at](std::shared_ptr<rtc::DataChannel> dc){
		if(dc->label() != "control"){
			// Nothing else is expected; ignoring rather than failing keeps the
			// worker forward compatible with an orchestrator that adds one.
			return;
		}

		{
			std::lock_guard<std::mutex> lock{control_mutex};
			control = dc;
		}

		dc->onOpen([dc, &cfg, started_at](){
			std::cout << "control channel open" << std::endl;

			auto hello = swarm::make_message<sch::MsgHelloNative>(
				msg::hello, cfg.worker_id, 0u);

			auto &cap = hello.get<"capability">();
			cap.get<"worker_id">().set(cfg.worker_id);
			cap.get<"kind">().set(std::string{wkind::native});
			cap.get<"schema_version">().set(std::string{schema_version});
			cap.get<"reported_at_ms">().set(now_ms());
			cap.get<"user_agent">().set(std::string{"kel-lbm-worker-native"});
			cap.get<"native">() = swarm::probe_native_capability();

			auto eo = swarm::encode_message<sch::MsgHelloNative>(hello);
			if(eo.is_error()){
				std::cerr << "could not encode hello\n";
				finish(1);
				return;
			}

			std::cout << "-> hello (" << eo.get_value().size() << " bytes)"
					  << std::endl;
			dc->send(eo.get_value());
		});

		dc->onMessage([dc, &cfg, started_at](rtc::message_variant raw){
			const std::string text = text_of(raw);
			if(text.empty()){
				return;
			}

			auto eo_hdr = swarm::peek_header(text);
			if(eo_hdr.is_error()){
				std::cerr << "malformed control message\n";
				finish(1);
				return;
			}

			const std::string type =
				eo_hdr.get_value().get<"type">().stl_string();
			std::cout << "<- " << type << std::endl;

			/**
			 * Step 2's orchestrator echoes rather than replying, so a hello
			 * coming back is what confirms the round trip. Once the registry
			 * lands this becomes a welcome, and both are accepted so the two
			 * can be developed in either order.
			 */
			if(type == msg::hello or type == msg::welcome){
				// Registration is done. If the run is also meant to cover
				// liveness, stay for the pings instead of leaving now.
				if(cfg.once and cfg.pongs_target == 0u){
					finish(0);
				}
				return;
			}

			if(type == msg::ping){
				auto eo_ping = swarm::decode_message<sch::MsgPing>(text);
				if(eo_ping.is_error()){
					std::cerr << "bad ping\n";
					return;
				}

				auto pong = swarm::make_message<sch::MsgPong>(
					msg::pong, cfg.worker_id,
					eo_ping.get_value().get<"seq">().get());
				pong.get<"sent_at_ms">().set(
					eo_ping.get_value().get<"sent_at_ms">().get());
				pong.get<"worker_now_ms">().set(now_ms());
				pong.get<"live">() = make_live(started_at);

				auto eo_out = swarm::encode_message<sch::MsgPong>(pong);
				if(eo_out.is_error()){
					return;
				}

				// Reply on the channel the message arrived on rather than
				// looking it up: same channel, no lock, no lifetime question.
				if(dc->isOpen()){
					dc->send(eo_out.get_value());
					const uint64_t sent = g_pongs_sent.fetch_add(1u) + 1u;
					if(cfg.pongs_target > 0u and sent >= cfg.pongs_target){
						std::cout << "answered " << sent << " ping(s)"
								  << std::endl;
						finish(0);
					}
				}
				return;
			}
		});

		dc->onClosed([](){
			std::cout << "control channel closed" << std::endl;
			finish(g_exit_code.load());
		});
	});

	ws->onMessage([pc](rtc::message_variant raw){
		const std::string text = text_of(raw);
		if(text.empty()){
			return;
		}

		auto eo_hdr = swarm::decode_message<sch::SignalHeader>(text);
		if(eo_hdr.is_error()){
			std::cerr << "malformed signalling message\n";
			return;
		}

		const std::string type = eo_hdr.get_value().get<"type">().stl_string();

		if(type == sig::offer or type == sig::answer){
			auto eo = swarm::decode_message<sch::SignalDescription>(text);
			if(eo.is_error()){
				std::cerr << "bad " << type << "\n";
				return;
			}
			try {
				// Auto negotiation turns setting a remote offer into an answer.
				pc->setRemoteDescription(
					rtc::Description{eo.get_value().get<"sdp">().stl_string(), type});
			} catch (const std::exception &e){
				std::cerr << "setRemoteDescription: " << e.what() << "\n";
			}
			return;
		}

		if(type == sig::candidate){
			auto eo = swarm::decode_message<sch::SignalCandidate>(text);
			if(eo.is_error()){
				std::cerr << "bad candidate\n";
				return;
			}
			try {
				pc->addRemoteCandidate(
					rtc::Candidate{eo.get_value().get<"candidate">().stl_string(),
								   eo.get_value().get<"mid">().stl_string()});
			} catch (const std::exception &e){
				std::cerr << "addRemoteCandidate: " << e.what() << "\n";
			}
			return;
		}
	});

	ws->onClosed([](){
		// Closing before the exchange completed is a failure; after it, the
		// exit code is already set and this is just teardown.
		finish(g_exit_code.load());
	});

	ws->onError([](std::string err){
		std::cerr << "signalling error: " << err << "\n";
		finish(1);
	});

	std::cout << "connecting to " << cfg.url << std::endl;
	try {
		ws->open(cfg.url);
	} catch (const std::exception &e){
		std::cerr << "[Error] Could not open " << cfg.url << ": " << e.what() << "\n";
		return 1;
	}

	{
		std::unique_lock<std::mutex> lock{g_mutex};
		const bool finished = g_cv.wait_for(
			lock, std::chrono::milliseconds{cfg.timeout_ms},
			[](){ return g_done.load(); });

		if(not finished){
			std::cerr << "[Error] Timed out after " << cfg.timeout_ms
					  << "ms without completing registration\n";
			g_exit_code.store(1);
		}
	}

	// Tear down before the callbacks' captures go out of scope.
	{
		std::lock_guard<std::mutex> lock{control_mutex};
		if(control){
			control->close();
		}
	}
	pc->close();
	ws->close();

	return g_exit_code.load();
}
