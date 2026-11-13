#pragma once

#include <rtc/rtc.hpp>

#include <memory>
#include <mutex>
#include <string>

namespace kel {
namespace lbm {
namespace swarm {

/**
 * One worker's connection, from the signalling socket through to the control
 * channel.
 *
 * Lifetime is the reason this is a type rather than three locals. libdatachannel
 * hands callbacks a reference to nothing in particular, and a PeerConnection
 * dropped at the end of the accept handler takes the whole negotiation with it.
 * The orchestrator therefore holds a shared_ptr per session and the callbacks
 * capture a weak_ptr, so a worker that disconnects mid-negotiation tears down
 * cleanly instead of leaving a callback pointing at freed state.
 *
 * Threading: every member here is touched from libdatachannel's threads, not
 * from main. The handles themselves are set once during setup and then only
 * read, and the mutable state behind them is libdatachannel's own problem. The
 * orchestrator's own mutable state lives in the registry, behind its mutex.
 */
struct session {
	/// Assigned by the orchestrator, not the worker. Stable for the connection.
	std::string id;

	/// Signalling. Stays open after negotiation so renegotiation is possible.
	std::shared_ptr<rtc::WebSocket> ws;

	std::shared_ptr<rtc::PeerConnection> pc;

	/**
	 * The control channel. Created by the orchestrator rather than the worker
	 * so that channel-open ordering is deterministic: the offer already
	 * contains it, so by the time the worker answers it knows the channel is
	 * coming and does not have to race onDataChannel against its own send.
	 */
	std::shared_ptr<rtc::DataChannel> dc;
};

} // namespace swarm
} // namespace lbm
} // namespace kel
