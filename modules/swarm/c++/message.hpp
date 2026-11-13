#pragma once

#include "capability.hpp"

#include <string_view>

/**
 * The worker/orchestrator control protocol.
 *
 * Every message is a JSON object carrying the same three leading members --
 * "type", "worker_id", "seq" -- followed by whatever that type adds. Decoding
 * happens in two stages (see wire.hpp): read MsgHeader to learn the type, then
 * decode the full typed struct.
 *
 * The reason the protocol is shaped this way rather than as a tagged union is
 * that sch::Union has no JSON codec. It turns out not to matter: the Struct
 * decoder skips keys it does not recognise, so reading a header out of a full
 * message is well defined and costs one extra parse of a short string.
 *
 * A versioning note that the strictness buys and charges for. Wire codecs run
 * strict, so a *missing* member is an error while an *unknown* member is
 * ignored. That means a newer sender talking to an older receiver is fine, but
 * an older sender talking to a newer receiver fails as soon as the newer side
 * adds a member. Adding a field to a message is therefore a breaking change in
 * one direction, which is what schema_version in the hello/welcome exchange is
 * for -- catch the mismatch deliberately instead of discovering it as a
 * confusing decode error.
 */
namespace kel {
namespace lbm {

namespace sch {

/**
 * Prepends the common members so each message type only states what it adds.
 */
template <typename... Members>
using Envelope = Struct<
	Member<String, "type">,
	Member<String, "worker_id">,
	Member<UInt64, "seq">,
	Members...
>;

/// Just the common members. Decoded first to learn which type follows.
using MsgHeader = Envelope<>;

/**
 * worker -> orchestrator. First message on the channel.
 *
 * MsgHello carries only the common capability fields, which is enough to learn
 * the worker kind. Decode it first, read capability.kind, then decode the same
 * text again as the matching variant below -- the backend section is skipped on
 * the first pass because unknown keys always are.
 *
 * The alternative, one hello naming every backend section, does not work: wire
 * codecs run strict, so it would oblige a browser to describe a CPU.
 */
using MsgHello = Envelope<Member<WorkerCapability, "capability">>;

using MsgHelloWebGl2 = Envelope<Member<WebGl2WorkerCapability, "capability">>;
using MsgHelloNative = Envelope<Member<NativeWorkerCapability, "capability">>;

/// orchestrator -> worker. Confirms registration and sets the heartbeat pace.
using MsgWelcome = Envelope<
	Member<UInt64, "heartbeat_ms">,
	Member<String, "schema_version">
>;

/// orchestrator -> worker.
using MsgPing = Envelope<Member<UInt64, "sent_at_ms">>;

/**
 * worker -> orchestrator. Echoes sent_at_ms so round trip time is measured
 * against the orchestrator's own clock and the two machines never have to agree
 * on what time it is.
 */
using MsgPong = Envelope<
	Member<UInt64, "sent_at_ms">,
	Member<UInt64, "worker_now_ms">,
	Member<WorkerLive, "live">
>;

/// worker -> orchestrator. Initialised and idle.
using MsgReady = Envelope<Member<WorkerLive, "live">>;

/// worker -> orchestrator. Unsolicited: a state change or an error.
using MsgStatus = Envelope<Member<WorkerLive, "live">>;

} // namespace sch

/**
 * Wire values for the "type" member.
 *
 * Deliberately strings rather than an integer enum: the orchestrator and the
 * worker are built from the same source today but will not always be, and a
 * string tag misread is a routing failure you can see in a log, where an
 * integer tag misread is a silently wrong payload.
 */
namespace msg {
inline constexpr std::string_view hello = "hello";
inline constexpr std::string_view welcome = "welcome";
inline constexpr std::string_view ping = "ping";
inline constexpr std::string_view pong = "pong";
inline constexpr std::string_view ready = "ready";
inline constexpr std::string_view status = "status";

/// orchestrator -> everyone. A run begins or ends.
inline constexpr std::string_view run_start = "run_start";
inline constexpr std::string_view run_end = "run_end";
} // namespace msg

/**
 * Values for WorkerLive's "state". The orchestrator owns stale and gone -- a
 * worker never reports those about itself, they are conclusions drawn from
 * missed heartbeats.
 */
namespace wstate {
inline constexpr std::string_view registered = "registered";
inline constexpr std::string_view ready = "ready";
inline constexpr std::string_view busy = "busy";
inline constexpr std::string_view error = "error";
inline constexpr std::string_view stale = "stale";
inline constexpr std::string_view gone = "gone";
} // namespace wstate

/**
 * Bumped whenever a message or capability schema changes shape. Compared on
 * hello/welcome so a mismatch is reported rather than hit as a decode failure.
 */
inline constexpr std::string_view schema_version = "1";

/// Worker kinds, the tag selecting which capability section is populated.
namespace wkind {
inline constexpr std::string_view webgl2 = "webgl2";
inline constexpr std::string_view native = "native";
} // namespace wkind

} // namespace lbm
} // namespace kel
