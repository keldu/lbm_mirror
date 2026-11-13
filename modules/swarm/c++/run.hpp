#pragma once

#include "message.hpp"

#include <kel/lbm/abstract/data.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <string_view>

/**
 * Run identity.
 *
 * There is none in the library today: outputs land in a directory named after
 * the example, with `<field>_<step>.vtk` inside, so a rerun silently
 * overwrites the one before it and nothing records what parameters produced
 * what. A swarm makes that worse rather than better -- several workers write
 * concurrently, and a result arriving for an unknown run has to be refusable.
 *
 * A run id is therefore the first thing the orchestrator mints and the thing
 * every transfer names. It is sortable, unique, and safe as a directory name:
 *
 *     20261112T094501Z-a3f19c
 *
 * UTC so that two machines agree, second resolution because runs are not
 * started faster than that by a human, and six hex digits of entropy so that
 * two started in the same second by a script still differ.
 */
namespace kel {
namespace lbm {
namespace sch {

using RunDescriptor = Struct<
	Member<String, "run_id">,
	Member<String, "name">,
	Member<UInt64, "created_at_ms">,
	Member<String, "schema_version">,
	/**
	 * Whether halo exchange was on. False until milestone 3, and recorded
	 * rather than assumed: slices run with clamped edges until then, so the
	 * seams are wrong, and a stored result should say so rather than leave
	 * someone to mistake a seam artefact for a solver bug years later.
	 */
	Member<Bool, "halo_exchange">,
	/// Opaque to everything here: whatever the admin endpoint was handed.
	Member<String, "params_json">
>;

} // namespace sch

namespace swarm {

namespace impl {

/**
 * Civil date from a day count since 1970-01-01, by Howard Hinnant's algorithm.
 *
 * Deliberately not gmtime_r: this is pure arithmetic, so it needs no locale,
 * no timezone database and no thread safety caveat, and it can be exercised
 * directly in a test rather than through whatever the host libc happens to
 * believe. It is also the same on a browser as on the orchestrator, which
 * matters when both ends format or parse an id.
 */
struct civil_date {
	int64_t year;
	uint32_t month;  ///< 1-12
	uint32_t day;    ///< 1-31
};

constexpr civil_date civil_from_days(int64_t z){
	z += 719468;
	const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
	const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int64_t y = static_cast<int64_t>(yoe) + era * 400;
	const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const uint64_t mp = (5 * doy + 2) / 153;
	const uint64_t d = doy - (153 * mp + 2) / 5 + 1;
	const uint64_t m = mp + (mp < 10 ? 3 : -9);
	return civil_date{y + (m <= 2 ? 1 : 0), static_cast<uint32_t>(m),
					  static_cast<uint32_t>(d)};
}

inline void append_padded(std::string &out, uint64_t value, uint32_t width){
	char buffer[20];
	uint32_t written = 0u;
	do {
		buffer[written++] = static_cast<char>('0' + (value % 10u));
		value /= 10u;
	} while(value > 0u and written < sizeof(buffer));

	for(uint32_t i = written; i < width; ++i){
		out.push_back('0');
	}
	for(uint32_t i = written; i > 0u; --i){
		out.push_back(buffer[i - 1u]);
	}
}

inline void append_hex(std::string &out, uint64_t value, uint32_t digits){
	static constexpr char table[] = "0123456789abcdef";
	for(uint32_t i = digits; i > 0u; --i){
		out.push_back(table[(value >> ((i - 1u) * 4u)) & 0xfu]);
	}
}

} // namespace impl

/**
 * Formats a run id.
 *
 * Takes the clock and the entropy rather than reading them, for the same
 * reason the registry takes `now_ms`: a test can then pin both and assert the
 * exact string, instead of matching a pattern and hoping.
 *
 * Only the low 24 bits of `entropy` are used.
 */
inline std::string make_run_id(uint64_t now_ms, uint64_t entropy){
	const int64_t seconds = static_cast<int64_t>(now_ms / 1000u);
	const int64_t days = (seconds >= 0) ? (seconds / 86400)
									    : ((seconds - 86399) / 86400);
	const uint64_t secs_of_day = static_cast<uint64_t>(seconds - days * 86400);

	const auto date = impl::civil_from_days(days);

	std::string out;
	out.reserve(24u);

	impl::append_padded(out, static_cast<uint64_t>(date.year), 4u);
	impl::append_padded(out, date.month, 2u);
	impl::append_padded(out, date.day, 2u);
	out.push_back('T');
	impl::append_padded(out, secs_of_day / 3600u, 2u);
	impl::append_padded(out, (secs_of_day / 60u) % 60u, 2u);
	impl::append_padded(out, secs_of_day % 60u, 2u);
	out.push_back('Z');
	out.push_back('-');
	impl::append_hex(out, entropy, 6u);

	return out;
}

/**
 * Whether a string could have come from make_run_id.
 *
 * The sink uses a run id as a directory name, so this is also the check that
 * keeps a hostile id from escaping the storage root -- there is no '/', no
 * '.', and nothing but a fixed shape of digits and hex.
 */
inline bool is_run_id(std::string_view id){
	if(id.size() != 23u){
		return false;
	}

	auto digit = [&](std::size_t i){ return id[i] >= '0' and id[i] <= '9'; };
	auto hex = [&](std::size_t i){
		return (id[i] >= '0' and id[i] <= '9') or (id[i] >= 'a' and id[i] <= 'f');
	};

	for(std::size_t i = 0u; i < 8u; ++i){
		if(not digit(i)){ return false; }
	}
	if(id[8] != 'T'){ return false; }
	for(std::size_t i = 9u; i < 15u; ++i){
		if(not digit(i)){ return false; }
	}
	if(id[15] != 'Z'){ return false; }
	if(id[16] != '-'){ return false; }
	for(std::size_t i = 17u; i < 23u; ++i){
		if(not hex(i)){ return false; }
	}

	return true;
}

} // namespace swarm

namespace sch {

using MsgRunStart = Envelope<Member<RunDescriptor, "run">>;
using MsgRunEnd = Envelope<Member<String, "run_id">, Member<String, "outcome">>;

} // namespace sch

namespace run_outcome {
inline constexpr std::string_view finished = "finished";
inline constexpr std::string_view cancelled = "cancelled";
inline constexpr std::string_view failed = "failed";
} // namespace run_outcome

} // namespace lbm
} // namespace kel
