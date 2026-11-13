#pragma once

#include <kel/lbm/abstract/schema.hpp>

#include <string_view>

/**
 * WebRTC signalling, carried over the WebSocket that precedes the data channel.
 *
 * This is a different protocol from the control messages in message.hpp and
 * deliberately so: signalling exists only to get a peer connection established,
 * after which everything moves to the data channel. It lives here rather than
 * in the orchestrator because both the browser worker and the native test
 * worker have to speak it, and one definition is the only way they cannot
 * disagree.
 *
 * The shapes mirror what a browser hands you, with one deliberate difference:
 * an ICE candidate arrives from JS as {candidate, sdpMid, sdpMLineIndex} and is
 * sent on as {type:"candidate", candidate, mid}. Normalising at the JS boundary
 * keeps the wire uniform -- every message here is a struct with a "type" -- at
 * the cost of three lines in the worker's transport glue.
 */
namespace kel {
namespace lbm {
namespace sch {

/// Stage one of the same two stage decode the control protocol uses.
using SignalHeader = Struct<Member<String, "type">>;

/// "offer" or "answer".
using SignalDescription = Struct<
	Member<String, "type">,
	Member<String, "sdp">
>;

/// "candidate". mid may be empty; libdatachannel treats that as unhinted.
using SignalCandidate = Struct<
	Member<String, "type">,
	Member<String, "candidate">,
	Member<String, "mid">
>;

} // namespace sch

namespace sig {
inline constexpr std::string_view offer = "offer";
inline constexpr std::string_view answer = "answer";
inline constexpr std::string_view candidate = "candidate";
} // namespace sig

} // namespace lbm
} // namespace kel
