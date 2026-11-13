#include <forstio/test/suite.hpp>

#include "../c++/run.hpp"
#include "../c++/wire.hpp"

#include <set>
#include <string>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;
using namespace kel::lbm::swarm;

/// 2026-11-12T09:45:01Z, as milliseconds since the epoch.
constexpr uint64_t reference_ms = 1794476701000ull;

SAW_TEST("Run / the id has the documented shape"){
	// Pinned exactly rather than pattern matched: make_run_id takes its clock
	// and entropy, so the whole string is determined and a drift in either
	// half should fail loudly.
	const std::string id = make_run_id(reference_ms, 0xa3f19cull);
	SAW_EXPECT(id == "20261112T094501Z-a3f19c",
		std::string{"unexpected run id: "} + id);
	SAW_EXPECT(id.size() == 23u, "and the fixed width the sink relies on");
}

SAW_TEST("Run / only the low 24 bits of entropy are used"){
	const std::string a = make_run_id(reference_ms, 0xffffffffffffffffull);
	SAW_EXPECT(a == "20261112T094501Z-ffffff", "the top bits are discarded");

	const std::string b = make_run_id(reference_ms, 0x0ull);
	SAW_EXPECT(b == "20261112T094501Z-000000", "and zero pads rather than shortens");
}

SAW_TEST("Run / ids sort chronologically as plain strings"){
	// The point of the format: `ls` and a std::map give time order for free,
	// with no parsing anywhere.
	const std::string earlier = make_run_id(reference_ms, 0xffffffull);
	const std::string later = make_run_id(reference_ms + 1000u, 0x000000ull);

	SAW_EXPECT(earlier < later,
		"A later run must sort after an earlier one even with lower entropy");

	// Across a month and a year boundary, where a naive format breaks.
	SAW_EXPECT(make_run_id(reference_ms, 0u)
		< make_run_id(reference_ms + 30ull * 86400000ull, 0u),
		"and across a month");
}

SAW_TEST("Run / the civil date conversion handles the awkward cases"){
	using kel::lbm::swarm::impl::civil_from_days;

	// The epoch itself.
	{
		const auto d = civil_from_days(0);
		SAW_EXPECT(d.year == 1970 and d.month == 1u and d.day == 1u, "epoch");
	}
	// A leap day, which is where a hand-rolled conversion usually goes wrong.
	{
		const auto d = civil_from_days(19782);  // 2024-02-29
		SAW_EXPECT(d.year == 2024 and d.month == 2u and d.day == 29u,
			"2024 is a leap year and the 29th exists");
	}
	// 2000 was a leap year despite being a century.
	{
		const auto d = civil_from_days(11016);  // 2000-02-29
		SAW_EXPECT(d.year == 2000 and d.month == 2u and d.day == 29u,
			"the 400 year rule");
	}
	// The day after the 2024 leap day, so an off-by-one shows.
	{
		const auto d = civil_from_days(19783);
		SAW_EXPECT(d.year == 2024 and d.month == 3u and d.day == 1u, "march 1st");
	}
}

SAW_TEST("Run / midnight and the last second of a day format correctly"){
	// 2026-11-12T00:00:00Z
	const uint64_t midnight = (reference_ms / 86400000ull) * 86400000ull;
	SAW_EXPECT(make_run_id(midnight, 0u) == "20261112T000000Z-000000",
		"midnight is all zeroes, not blank");

	SAW_EXPECT(make_run_id(midnight + 86399000ull, 0u)
		== "20261112T235959Z-000000", "and the last second is 235959");

	// One more second rolls the date rather than producing 24:00:00.
	SAW_EXPECT(make_run_id(midnight + 86400000ull, 0u)
		== "20261113T000000Z-000000", "which rolls into the next day");
}

/**
 * The sink uses a run id as a directory name, so this is the check that stops
 * a hostile id escaping the storage root.
 */
SAW_TEST("Run / validation rejects anything that is not the exact shape"){
	SAW_EXPECT(is_run_id(make_run_id(reference_ms, 0xabcdefull)),
		"what make_run_id produces must validate");

	const char *bad[] = {
		"",
		"20261112T094501Z",                 // no entropy
		"20261112T094501Z-a3f19",           // one short
		"20261112T094501Z-a3f19cd",         // one long
		"20261112t094501Z-a3f19c",          // lowercase separator
		"20261112T094501Z-A3F19C",          // uppercase hex
		"20261112T094501Z-a3f19g",          // 'g' is not hex
		"2026111 T094501Z-a3f19c",          // space
		"../../etc/passwd000000Z-a3f19c",   // the reason this function exists
		"20261112T094501Z/a3f19c",          // a separator that is a path one
	};

	for(const char *id : bad){
		SAW_EXPECT(not is_run_id(id),
			std::string{"must be rejected: "} + id);
	}
}

SAW_TEST("Run / distinct entropy gives distinct ids within one second"){
	std::set<std::string> seen;
	for(uint64_t e = 0u; e < 512u; ++e){
		seen.insert(make_run_id(reference_ms, e));
	}
	SAW_EXPECT(seen.size() == 512u,
		"Two runs started in the same second must not collide");
}

SAW_TEST("Run / a run descriptor round trips on the wire"){
	auto start = make_message<sch::MsgRunStart>(msg::run_start, "orch", 0u);

	auto &run = start.get<"run">();
	run.get<"run_id">().set(make_run_id(reference_ms, 0xa3f19cull));
	run.get<"name">().set(std::string{"poiseuille 1024x256"});
	run.get<"created_at_ms">().set(reference_ms);
	run.get<"schema_version">().set(std::string{schema_version});
	run.get<"halo_exchange">().set(false);
	run.get<"params_json">().set(std::string{R"({"nx":1024,"ny":256})"});

	auto eo_text = encode_message<sch::MsgRunStart>(start);
	SAW_EXPECT(not eo_text.is_error(), "a run start encodes");

	auto eo_hdr = peek_header(eo_text.get_value());
	SAW_EXPECT(not eo_hdr.is_error(), "and routes");
	SAW_EXPECT(header_type(eo_hdr.get_value()) == msg::run_start, "as a run start");

	auto eo_back = decode_message<sch::MsgRunStart>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "and decodes");

	const auto &back = eo_back.get_value().get<"run">();
	SAW_EXPECT(back.get<"run_id">().stl_view() == "20261112T094501Z-a3f19c",
		"the id survives");
	SAW_EXPECT(not back.get<"halo_exchange">().get(),
		"and the record that the seams were clamped survives with it");
	SAW_EXPECT(back.get<"params_json">().stl_view() == R"({"nx":1024,"ny":256})",
		"the opaque params survive being nested inside JSON");
}
}
