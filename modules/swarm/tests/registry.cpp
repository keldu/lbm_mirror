#include <forstio/test/suite.hpp>

#include "../c++/message.hpp"
#include "../c++/registry.hpp"

#include <string>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;
using namespace kel::lbm::swarm;

/**
 * The registry takes `now_ms` at every entry point rather than reading a clock,
 * so these tests drive a worker from healthy to dropped in microseconds. Doing
 * this against a real connection would mean sleeping through several heartbeat
 * intervals and hoping the timing held.
 */

data<sch::WorkerLive> some_live(){
	data<sch::WorkerLive> live;
	live.get<"state">().set(std::string{wstate::ready});
	live.get<"uptime_ms">().set(0u);
	live.get<"steps_done">().set(0u);
	live.get<"last_error">().set(std::string{""});
	return live;
}

/**
 * Registers one worker into an existing registry. Fills in place rather than
 * returning one: registry holds a mutex, so it is neither copyable nor movable.
 */
void register_one(registry &reg){
	reg.add_session("w1", 1000u);
	reg.mark_registered("w1", "alpha", std::string{wkind::native}, 1000u,
						"test worker", 1000u);
}

worker_record only(const registry &reg){
	auto snap = reg.snapshot();
	return snap.empty() ? worker_record{} : snap.front();
}

SAW_TEST("Registry / a session starts in signalling and is not pinged"){
	registry reg;
	reg.add_session("w1", 100u);

	SAW_EXPECT(reg.size() == 1u, "The session is tracked");
	SAW_EXPECT(only(reg).state == worker_state::signalling,
		"A fresh session has not said hello yet");

	// Crucially the thresholds must not apply before a hello. A worker still
	// negotiating is not unhealthy, and counting misses against it would drop
	// it before it ever had a channel to answer on.
	for(uint64_t i = 0u; i < 20u; ++i){
		auto result = reg.tick(200u + i);
		SAW_EXPECT(result.to_ping.empty(), "A signalling session is not pinged");
		SAW_EXPECT(result.dropped.empty(), "A signalling session is not dropped");
	}
	SAW_EXPECT(reg.size() == 1u, "It is still there after many ticks");
}

SAW_TEST("Registry / registration records the capability"){
	registry reg;
	register_one(reg);
	const auto rec = only(reg);

	SAW_EXPECT(rec.state == worker_state::registered, "hello moves it on");
	SAW_EXPECT(rec.worker_id == "alpha", "The self reported name is kept");
	SAW_EXPECT(rec.kind == wkind::native, "The kind is kept");
	SAW_EXPECT(rec.capacity_cells == 1000u, "The capacity is kept");
	SAW_EXPECT(rec.session_id == "w1", "Keyed on the assigned id");
}

SAW_TEST("Registry / registering an unknown session fails rather than creating one"){
	registry reg;
	SAW_EXPECT(not reg.mark_registered("ghost", "a", "native", 1u, "", 1u),
		"A hello from a session that has gone must not resurrect it");
	SAW_EXPECT(reg.size() == 0u, "and must not invent a record");
}

SAW_TEST("Registry / a ping is outstanding until its pong"){
	registry reg;
	register_one(reg);

	auto t1 = reg.tick(2000u);
	SAW_EXPECT(t1.to_ping.size() == 1u, "One worker, one ping");
	SAW_EXPECT(t1.to_ping.front().seq == 1u, "Sequences start at one");
	SAW_EXPECT(only(reg).outstanding_pings == 1u, "The ping is outstanding");

	reg.record_pong("w1", 1u, 2000u, some_live(), 2020u);
	SAW_EXPECT(only(reg).outstanding_pings == 0u, "The pong clears it");
	SAW_EXPECT(only(reg).pongs == 1u, "and is counted");
}

SAW_TEST("Registry / round trip is measured against our own clock"){
	registry reg;
	register_one(reg);

	reg.tick(2000u);
	// The worker echoes the timestamp we sent, so no clock agreement needed.
	reg.record_pong("w1", 1u, 2000u, some_live(), 2040u);
	SAW_EXPECT(only(reg).rtt_ewma_ms == 40.0,
		"The first sample seeds the average exactly");

	// Second sample at 80ms: 0.25*80 + 0.75*40 = 50.
	reg.tick(3000u);
	reg.record_pong("w1", 2u, 3000u, some_live(), 3080u);
	const double rtt = only(reg).rtt_ewma_ms;
	SAW_EXPECT(rtt > 49.9 and rtt < 50.1, "Smoothing weights the history");
}

SAW_TEST("Registry / a clock that goes backwards does not produce a negative rtt"){
	registry reg;
	register_one(reg);

	reg.tick(2000u);
	// Echoed timestamp later than now. Should clamp, not underflow into a
	// huge positive via unsigned subtraction.
	reg.record_pong("w1", 1u, 5000u, some_live(), 2000u);
	SAW_EXPECT(only(reg).rtt_ewma_ms == 0.0, "A backwards clock reads as zero");
}

/**
 * The transition that actually matters, and the one that is tedious to provoke
 * against a live connection.
 */
SAW_TEST("Registry / stale then dropped, at the configured thresholds"){
	worker_thresholds limits;
	limits.stale_after = 2u;
	limits.drop_after = 4u;

	registry reg{limits};
	register_one(reg);
	uint64_t t = 2000u;

	// Beat 1 and 2: outstanding climbs, still considered healthy.
	auto b1 = reg.tick(t += 100u);
	SAW_EXPECT(b1.newly_stale.empty(), "One miss is not yet doubtful");
	SAW_EXPECT(only(reg).state == worker_state::registered, "still registered");

	auto b2 = reg.tick(t += 100u);
	SAW_EXPECT(b2.newly_stale.empty(), "Two outstanding, threshold not crossed yet");

	// Beat 3 sees outstanding == 2, which meets stale_after.
	auto b3 = reg.tick(t += 100u);
	SAW_EXPECT(b3.newly_stale.size() == 1u, "Crossing into stale is reported once");
	SAW_EXPECT(b3.newly_stale.front() == "w1", "and names the session");
	SAW_EXPECT(only(reg).state == worker_state::stale, "and is recorded");

	// Reported once, not every beat afterwards -- otherwise the log fills up.
	auto b4 = reg.tick(t += 100u);
	SAW_EXPECT(b4.newly_stale.empty(), "Stale is announced once, not repeatedly");
	SAW_EXPECT(only(reg).state == worker_state::stale, "and it stays stale");

	// Beat 5 sees outstanding == 4, which meets drop_after.
	auto b5 = reg.tick(t += 100u);
	SAW_EXPECT(b5.dropped.size() == 1u, "It is dropped at the threshold");
	SAW_EXPECT(b5.dropped.front().worker_id == "alpha",
		"The dropped record is handed back so the caller can log it");
	SAW_EXPECT(reg.size() == 0u, "and removed from the registry");
	SAW_EXPECT(b5.to_ping.empty(), "A dropped session is not also pinged");
}

SAW_TEST("Registry / answering again clears stale"){
	worker_thresholds limits;
	limits.stale_after = 2u;
	limits.drop_after = 10u;

	registry reg{limits};
	register_one(reg);
	uint64_t t = 2000u;

	reg.tick(t += 100u);
	reg.tick(t += 100u);
	auto b3 = reg.tick(t += 100u);
	SAW_EXPECT(only(reg).state == worker_state::stale, "It went stale");

	// The worker comes back, answering the ping that is actually outstanding.
	reg.record_pong("w1", b3.to_ping.front().seq, t, some_live(), t + 5u);

	SAW_EXPECT(only(reg).state == worker_state::registered,
		"Answering clears the doubt");
	SAW_EXPECT(only(reg).outstanding_pings == 0u, "and resets the miss count");
}

SAW_TEST("Registry / recovery does not silently restore readiness"){
	// Readiness is something only the worker can assert, so a recovered worker
	// goes back to registered and has to say 'ready' again.
	worker_thresholds limits;
	limits.stale_after = 1u;
	limits.drop_after = 10u;

	registry reg{limits};
	register_one(reg);
	reg.mark_ready("w1", some_live(), 1500u);
	SAW_EXPECT(only(reg).state == worker_state::ready, "It was ready");

	reg.tick(2000u);
	auto b2 = reg.tick(2100u);
	SAW_EXPECT(only(reg).state == worker_state::stale, "Then it went quiet");

	reg.record_pong("w1", b2.to_ping.front().seq, 2100u, some_live(), 2110u);
	SAW_EXPECT(only(reg).state == worker_state::registered,
		"Recovery must not reassert readiness on the worker's behalf");
}

/**
 * A worker consistently one beat behind would otherwise look perfectly healthy:
 * every tick adds a miss, every late pong clears it, and the miss count never
 * reaches the threshold.
 */
SAW_TEST("Registry / a stale sequence pong does not clear the miss count"){
	registry reg;
	register_one(reg);

	auto b1 = reg.tick(2000u);
	const uint64_t old_seq = b1.to_ping.front().seq;

	reg.tick(2100u);
	reg.tick(2200u);
	SAW_EXPECT(only(reg).outstanding_pings == 3u, "Three pings outstanding");

	// Answer the first one, long after it was superseded.
	reg.record_pong("w1", old_seq, 2000u, some_live(), 2250u);

	SAW_EXPECT(only(reg).outstanding_pings == 3u,
		"A pong for a superseded ping must not clear the misses");
	SAW_EXPECT(only(reg).pongs == 0u, "nor count as an answer");
	SAW_EXPECT(only(reg).last_seen_ms == 2250u,
		"but it does prove the channel is alive");
}

SAW_TEST("Registry / removal returns the record once"){
	registry reg;
	register_one(reg);

	auto gone = reg.remove("w1");
	SAW_EXPECT(gone.has_value(), "Removing returns what was removed");
	SAW_EXPECT(gone->worker_id == "alpha", "with its contents intact");
	SAW_EXPECT(reg.size() == 0u, "and empties the registry");

	// Idempotent: several teardown paths converge on this and whichever
	// arrives first should win without the others erroring.
	SAW_EXPECT(not reg.remove("w1").has_value(), "A second removal is a no-op");
}

SAW_TEST("Registry / many workers are ticked independently"){
	worker_thresholds limits;
	limits.stale_after = 2u;
	limits.drop_after = 3u;

	registry reg{limits};
	for(uint64_t i = 0u; i < 5u; ++i){
		const std::string id = "w" + std::to_string(i);
		reg.add_session(id, 1000u);
		reg.mark_registered(id, "worker" + std::to_string(i), "native",
							1000u * i, "", 1000u);
	}

	uint64_t t = 1000u;
	for(int beat = 0; beat < 3; ++beat){
		auto result = reg.tick(t += 100u);
		// w3 keeps answering; everyone else goes quiet.
		for(const auto &target : result.to_ping){
			if(target.session_id == "w3"){
				reg.record_pong("w3", target.seq, t, some_live(), t + 1u);
			}
		}
	}

	auto result = reg.tick(t += 100u);
	SAW_EXPECT(result.dropped.size() == 4u, "The four silent workers are dropped");
	SAW_EXPECT(reg.size() == 1u, "The responsive one survives");
	SAW_EXPECT(only(reg).session_id == "w3", "and it is the right one");
}

SAW_TEST("Registry / the table renders without a worker and with one"){
	registry reg;
	SAW_EXPECT(render_table(reg.snapshot(), 0u).find("(no workers)")
		!= std::string::npos, "An empty registry says so");

	registry one;
	register_one(one);
	one.tick(2000u);
	one.record_pong("w1", 1u, 2000u, some_live(), 2010u);

	const std::string table = render_table(one.snapshot(), 3000u);
	SAW_EXPECT(table.find("alpha") != std::string::npos, "The worker is listed");
	SAW_EXPECT(table.find("native") != std::string::npos, "with its kind");
	SAW_EXPECT(table.find("10.0ms") != std::string::npos, "and its round trip");
	// 1000 cells renders as a bare count, not scaled.
	SAW_EXPECT(table.find("1.0k") != std::string::npos,
		"and a human readable capacity");
}
}
