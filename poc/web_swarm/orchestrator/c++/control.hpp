#pragma once

#include "orchestrator.hpp"
#include <kel/lbm/swarm/registry.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace kel {
namespace lbm {
namespace swarm {

struct control_config {
	uint64_t heartbeat_ms = 2000u;
	registry::thresholds limits;
};

/**
 * The control protocol on top of the transport.
 *
 * Kept apart from `orchestrator` so each can be reasoned about alone: the
 * transport knows nothing about hellos, and this knows nothing about ICE.
 *
 * Owns the heartbeat thread. libdatachannel provides no timers, so liveness is
 * a plain ticker -- which is fine, since the work it does per beat is a lock, a
 * vector copy and some sends.
 */
class control {
public:
	control(orchestrator &orch, control_config cfg);
	~control();

	control(const control &) = delete;
	control &operator=(const control &) = delete;

	/// Installs the handlers on the orchestrator. Call before listen().
	void attach();

	void start_heartbeat();
	void stop_heartbeat();

	registry &workers() { return registry_; }

	/// Emitted for anything worth a line in the log.
	void on_event(std::function<void(const std::string &)> handler);

private:
	void handle_message(const std::shared_ptr<session> &sess,
						const std::string &text);
	void handle_hello(const std::shared_ptr<session> &sess,
					  const std::string &text);
	void heartbeat_loop();
	void emit(const std::string &line);

	orchestrator &orch_;
	control_config cfg_;
	registry registry_;

	std::thread ticker_;
	std::mutex tick_mutex_;
	std::condition_variable tick_cv_;
	std::atomic<bool> running_{false};

	std::function<void(const std::string &)> on_event_;
};

} // namespace swarm
} // namespace lbm
} // namespace kel
