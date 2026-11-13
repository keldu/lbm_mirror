#pragma once

#include "message.hpp"

#include <kel/lbm/abstract/data.hpp>
#include <kel/lbm/abstract/json.hpp>

#include <string>
#include <string_view>

/**
 * Encoding and decoding control messages.
 *
 * Everything here runs the JSON codec in strict mode. That is not the codec's
 * default, and the default is wrong for a wire: codec_config<enc::Json>::strict
 * is false so that config files can state only their deviations, which means a
 * worker sending "{}" would decode cleanly into a default constructed struct.
 * A registration that silently becomes all zeroes is precisely the failure this
 * protocol must not have, so strict is set explicitly on every wire codec.
 *
 * Strict only governs *missing* members. Unknown members are skipped by the
 * Struct decoder, which is what makes the two stage decode below work and what
 * lets a newer worker talk to an older orchestrator.
 */
namespace kel {
namespace lbm {
namespace swarm {

namespace impl {
template <typename Schema>
codec<Schema, enc::Json> make_wire_codec(){
	codec_config<enc::Json> cfg;
	cfg.strict = true;
	// Control messages nest at most capability -> webgl2 -> feasible -> plan,
	// so the default guard against pathological input is already generous.
	return codec<Schema, enc::Json>{cfg};
}
} // namespace impl

/**
 * Serialise a message to the text that goes on the data channel.
 */
template <typename Schema>
error_or<std::string> encode_message(const data<Schema> &msg){
	auto wire_codec = impl::make_wire_codec<Schema>();

	data<Schema, enc::Json> out;
	auto eov = wire_codec.encode(msg, out);
	if(eov.is_error()){
		return std::move(eov.get_error());
	}

	return out.stl_string();
}

/**
 * Decode text into a known message type. Fails if any member is absent.
 */
template <typename Schema>
error_or<data<Schema>> decode_message(std::string_view text){
	auto wire_codec = impl::make_wire_codec<Schema>();

	data<Schema, enc::Json> in{text};
	data<Schema> out;
	auto eov = wire_codec.decode(in, out);
	if(eov.is_error()){
		return std::move(eov.get_error());
	}

	return out;
}

/**
 * Stage one: read just the common members so the caller learns which type to
 * decode. The payload is skipped rather than parsed into anything, so this
 * succeeds for every message type and fails only on genuinely malformed input
 * or a message missing the common members.
 */
inline error_or<data<sch::MsgHeader>> peek_header(std::string_view text){
	return decode_message<sch::MsgHeader>(text);
}

/**
 * Fills in the members every message shares. Kept in one place so a message
 * built by the orchestrator and one built by the worker cannot disagree about
 * the shape of the envelope.
 */
template <typename Schema>
void set_envelope(data<Schema> &msg, std::string_view type,
				  std::string_view worker_id, uint64_t seq){
	msg.template get<"type">().set(std::string{type});
	msg.template get<"worker_id">().set(std::string{worker_id});
	msg.template get<"seq">().set(seq);
}

/**
 * Convenience for the common shape: build a message, stamp the envelope.
 */
template <typename Schema>
data<Schema> make_message(std::string_view type, std::string_view worker_id,
						  uint64_t seq){
	data<Schema> msg;
	set_envelope(msg, type, worker_id, seq);
	return msg;
}

/// Reads the "type" of an already decoded header.
inline std::string_view header_type(const data<sch::MsgHeader> &hdr){
	return hdr.template get<"type">().stl_view();
}

/// Reads the "worker_id" of an already decoded header.
inline std::string_view header_worker_id(const data<sch::MsgHeader> &hdr){
	return hdr.template get<"worker_id">().stl_view();
}

} // namespace swarm
} // namespace lbm
} // namespace kel
