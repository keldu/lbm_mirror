#include "control.hpp"
#include "orchestrator.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

/**
 * The swarm orchestrator.
 *
 * Accepts workers over a WebSocket, negotiates a peer connection, opens a
 * control data channel, registers whatever capability the worker reports, and
 * keeps the connection under a heartbeat so liveness is a fact rather than an
 * assumption.
 *
 * There is no event loop of our own. libdatachannel runs its own threads and
 * the heartbeat runs on another, so main's whole job is to start things, print
 * what happens, and wait for a signal.
 */
namespace {

std::mutex g_wait_mutex;
std::condition_variable g_wait_cv;
std::atomic<bool> g_running{true};
std::atomic<bool> g_dump_requested{false};

/**
 * Signal handlers may only touch lock free atomics, so these set a flag and
 * poke the condition variable. The notify is outside what the standard blesses
 * in a handler, but the predicate is re-checked on a timed wakeup, so a lost
 * notify costs at most the poll interval rather than a hang.
 */
extern "C" void handle_stop(int){
	g_running.store(false);
	g_wait_cv.notify_all();
}

extern "C" void handle_dump(int){
	g_dump_requested.store(true);
	g_wait_cv.notify_all();
}

uint64_t now_ms(){
	using namespace std::chrono;
	return static_cast<uint64_t>(
		duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void print_usage(const char *argv0){
	std::cout
		<< "Usage: " << argv0 << " [options]\n"
		<< "\n"
		<< "  --port <n>          Port for the signalling WebSocket\n"
		<< "                      (default 8080). 0 asks the OS for a free one.\n"
		<< "  --stun <url>        STUN server, e.g.\n"
		<< "                      stun:stun.l.google.com:19302. Not needed on\n"
		<< "                      localhost or a LAN.\n"
		<< "  --heartbeat <ms>    Ping interval (default 2000).\n"
		<< "  --stale-after <n>   Missed pings before a worker is called stale\n"
		<< "                      (default 3).\n"
		<< "  --drop-after <n>    Missed pings before it is dropped (default 6).\n"
		<< "  --monitor <ms>      Print the worker table this often. 0 disables\n"
		<< "                      (default 5000). Nothing is printed while no\n"
		<< "                      workers are connected.\n"
		<< "  --help              This text.\n"
		<< "\n"
		<< "SIGUSR1 prints the worker table immediately.\n"
		<< "\n"
		<< "The worker page is not served from here -- a WebSocket server is\n"
		<< "not an HTTP server. Serve it separately, e.g.\n"
		<< "  python3 -m http.server 8000\n";
}

} // namespace

int main(int argc, char **argv){
	using namespace kel::lbm;

	swarm::orchestrator_config ocfg;
	swarm::control_config ccfg;
	uint64_t monitor_ms = 5000u;

	for(int i = 1; i < argc; ++i){
		const std::string arg{argv[i]};

		if(arg == "--help" or arg == "-h"){
			print_usage(argv[0]);
			return 0;
		}
		if(arg == "--port" and (i + 1) < argc){
			ocfg.port = static_cast<uint16_t>(std::strtoul(argv[++i], nullptr, 10));
			continue;
		}
		if(arg == "--stun" and (i + 1) < argc){
			ocfg.stun_url = argv[++i];
			continue;
		}
		if(arg == "--heartbeat" and (i + 1) < argc){
			ccfg.heartbeat_ms = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}
		if(arg == "--stale-after" and (i + 1) < argc){
			ccfg.limits.stale_after = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}
		if(arg == "--drop-after" and (i + 1) < argc){
			ccfg.limits.drop_after = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}
		if(arg == "--monitor" and (i + 1) < argc){
			monitor_ms = std::strtoull(argv[++i], nullptr, 10);
			continue;
		}

		std::cerr << "Unknown or incomplete option: " << arg << "\n\n";
		print_usage(argv[0]);
		return 2;
	}

	if(ccfg.heartbeat_ms == 0u){
		std::cerr << "--heartbeat must be greater than zero\n";
		return 2;
	}
	if(ccfg.limits.drop_after < ccfg.limits.stale_after){
		// Otherwise a worker is dropped before it can ever be reported stale,
		// which makes the stale state unreachable and the log misleading.
		std::cerr << "--drop-after must be at least --stale-after\n";
		return 2;
	}

	std::signal(SIGINT, handle_stop);
	std::signal(SIGTERM, handle_stop);
	std::signal(SIGUSR1, handle_dump);

	swarm::orchestrator orch{ocfg};
	swarm::control ctl{orch, ccfg};

	ctl.on_event([](const std::string &line){
		std::cout << line << std::endl;
	});

	ctl.attach();

	uint16_t port = 0u;
	try {
		port = orch.listen();
	} catch (const std::exception &e){
		std::cerr << "[Error] Could not listen: " << e.what() << "\n";
		return 1;
	}

	ctl.start_heartbeat();

	std::cout << "orchestrator listening on ws://0.0.0.0:" << port << "\n"
			  << "heartbeat " << ccfg.heartbeat_ms << "ms, stale after "
			  << ccfg.limits.stale_after << " misses, drop after "
			  << ccfg.limits.drop_after << "\n"
			  << "ctrl-c to stop, SIGUSR1 for the worker table" << std::endl;

	uint64_t last_monitor = now_ms();

	{
		std::unique_lock<std::mutex> lock{g_wait_mutex};
		while(g_running.load()){
			g_wait_cv.wait_for(lock, std::chrono::milliseconds{250},
							   [](){ return not g_running.load()
											or g_dump_requested.load(); });

			const bool asked = g_dump_requested.exchange(false);
			const uint64_t t = now_ms();
			const bool due = (monitor_ms > 0u) and ((t - last_monitor) >= monitor_ms);

			if(not g_running.load()){
				break;
			}
			if(not asked and not due){
				continue;
			}

			last_monitor = t;

			auto snapshot = ctl.workers().snapshot();
			// An idle orchestrator should be quiet; a periodic table of
			// nothing is just log noise. An explicit SIGUSR1 always prints,
			// because then someone is actually asking.
			if(snapshot.empty() and not asked){
				continue;
			}

			std::cout << "\n" << snapshot.size() << " worker(s):\n"
					  << swarm::render_table(snapshot, t) << std::endl;
		}
	}

	std::cout << "\nstopping" << std::endl;
	ctl.stop_heartbeat();
	orch.stop();

	return 0;
}
