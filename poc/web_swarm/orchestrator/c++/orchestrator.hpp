#pragma once

#include "session.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace kel {
namespace lbm {
namespace swarm {

struct orchestrator_config {
	uint16_t port = 8080u;

	/**
	 * STUN is not needed on localhost or a LAN -- host candidates are enough,
	 * and they are gathered without any server. The slot is here so NAT
	 * traversal is a configuration change later rather than a code change.
	 */
	std::string stun_url;
};

/**
 * Accepts workers and holds their connections open.
 *
 * This is the first code in the repository with concurrency in it, so the rule
 * is worth stating rather than inferring: libdatachannel owns the threads and
 * delivers every callback on one of them. The only mutable state the
 * orchestrator itself owns is the session map, and it is guarded by one mutex.
 * Nothing else here is shared. Resist growing that.
 *
 * A deliberate non-feature: no static file serving. The page has to come from
 * somewhere else (python3 -m http.server over the nix output is fine for
 * development), because a WebSocket server is not an HTTP server and pretending
 * otherwise would mean carrying a second protocol for no benefit.
 */
class orchestrator {
public:
	/**
	 * Called with the session and the decoded text of each control message.
	 * Handling lives outside this class so the transport can be tested without
	 * a protocol and the protocol without a transport.
	 */
	using message_handler =
		std::function<void(const std::shared_ptr<session> &, const std::string &)>;

	explicit orchestrator(orchestrator_config cfg);
	~orchestrator();

	orchestrator(const orchestrator &) = delete;
	orchestrator &operator=(const orchestrator &) = delete;

	/// Starts listening. Returns the port actually bound, which differs from
	/// the requested one when the config asked for 0.
	uint16_t listen();

	void on_message(message_handler handler);

	/// Invoked once a session's control channel is open and usable.
	void on_channel_open(std::function<void(const std::shared_ptr<session> &)> handler);

	/// Invoked when a session goes away, however it goes away.
	void on_channel_closed(std::function<void(const std::shared_ptr<session> &)> handler);

	/// Sends text on a session's control channel. False if it is not open.
	bool send(const std::shared_ptr<session> &sess, const std::string &text);

	/// Number of sessions currently held. Takes the lock.
	std::size_t session_count() const;

	/// Runs `fn` for each session under the lock. Keep it short.
	void for_each_session(
		const std::function<void(const std::shared_ptr<session> &)> &fn) const;

	/**
	 * Resolves a session id to something sendable, or null if it has gone.
	 *
	 * The registry deals in ids alone so it can stay transport free, which
	 * leaves this as the one place that maps an id back to a connection.
	 */
	std::shared_ptr<session> find_session(const std::string &id) const;

	void stop();

private:
	void accept_client(std::shared_ptr<rtc::WebSocket> ws);
	void begin_negotiation(const std::shared_ptr<session> &sess);
	void handle_signal(const std::shared_ptr<session> &sess, const std::string &text);
	void drop_session(const std::string &id);

	std::string next_session_id();

	orchestrator_config cfg_;

	std::unique_ptr<rtc::WebSocketServer> server_;

	mutable std::mutex mutex_;
	std::map<std::string, std::shared_ptr<session>> sessions_;
	uint64_t next_id_ = 0u;

	message_handler on_message_;
	std::function<void(const std::shared_ptr<session> &)> on_open_;
	std::function<void(const std::shared_ptr<session> &)> on_closed_;
};

} // namespace swarm
} // namespace lbm
} // namespace kel
