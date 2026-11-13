#pragma once

#include "capability.hpp"

#include <kel/lbm/abstract/data.hpp>

#include <cstdint>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

/**
 * Who is connected, how healthy they are, and how much they can hold.
 *
 * Deliberately free of any transport. The registry never sees a socket, a peer
 * connection or a data channel -- it is given a session id and timestamps, and
 * it answers with decisions. Two reasons:
 *
 *   1. modules/swarm is compiled by em++ as well as by the host compiler, and
 *      libdatachannel does not exist in a browser. Anything in here that
 *      touched it would make the worker unbuildable.
 *   2. The liveness state machine is the part most worth testing and the part
 *      least convenient to test through a real connection. Every entry point
 *      takes `now_ms` explicitly rather than reading a clock, so a test can
 *      drive a worker from healthy to dropped in a few microseconds and assert
 *      each transition on the way.
 *
 * Resolving a session id back to something sendable is the caller's job.
 */
namespace kel {
namespace lbm {
namespace swarm {

enum class worker_state {
	/// Connected, peer connection up, but no hello yet.
	signalling,
	/// Hello received and accepted.
	registered,
	/// Worker says it is initialised and idle.
	ready,
	/// Missed enough heartbeats to be doubtful, not enough to give up.
	stale,
};

inline const char *to_string(worker_state state){
	switch(state){
		case worker_state::signalling: return "signalling";
		case worker_state::registered: return "registered";
		case worker_state::ready:      return "ready";
		case worker_state::stale:      return "stale";
	}
	return "?";
}

/**
 * Liveness thresholds.
 *
 * At namespace scope rather than nested in registry so it is a complete type
 * where the constructor default argument needs it.
 */
struct worker_thresholds {
	/// Missed pings before a worker is called stale.
	uint64_t stale_after = 3u;
	/// Missed pings before it is dropped entirely.
	uint64_t drop_after = 6u;
};

/**
 * What the orchestrator knows about one worker.
 *
 * Note which id is which. session_id is assigned by the orchestrator and is
 * unique by construction; worker_id is whatever the worker called itself and is
 * neither unique nor trustworthy. The registry is keyed on the former and
 * merely carries the latter, so two workers choosing the same name is a display
 * annoyance rather than a collision.
 */
struct worker_record {
	std::string session_id;
	std::string worker_id;
	std::string kind;

	worker_state state = worker_state::signalling;

	uint64_t connected_at_ms = 0u;
	uint64_t registered_at_ms = 0u;
	uint64_t last_seen_ms = 0u;

	/// Sequence of the most recent ping, so a late pong can be told from a
	/// current one rather than silently resetting the miss counter.
	uint64_t ping_seq = 0u;
	uint64_t outstanding_pings = 0u;
	uint64_t pongs = 0u;

	/// Exponentially weighted mean round trip, milliseconds. Negative until
	/// the first pong lands.
	double rtt_ewma_ms = -1.0;

	/**
	 * Roughly how many lattice cells this worker could hold, derived from what
	 * it reported. The whole point of a capability is to answer this, so it is
	 * computed once on registration rather than recomputed per query.
	 */
	uint64_t capacity_cells = 0u;

	/// Short human readable form of the capability, for the monitor.
	std::string summary;

	/// Last live block the worker sent, on a pong or a status.
	data<sch::WorkerLive> live;
};

/**
 * The orchestrator's only shared mutable state.
 *
 * libdatachannel delivers callbacks on its own threads and the heartbeat runs
 * on another, so everything here is behind one mutex. The interface is
 * deliberately coarse -- whole operations rather than accessors -- so callers
 * cannot hold a reference to a record past the lock, which is the usual way a
 * design like this goes wrong.
 */
class registry {
public:
	using thresholds = worker_thresholds;

	explicit registry(worker_thresholds limits = {}) : limits_{limits} {}

	void add_session(const std::string &session_id, uint64_t now_ms){
		std::lock_guard<std::mutex> lock{mutex_};

		worker_record rec;
		rec.session_id = session_id;
		rec.state = worker_state::signalling;
		rec.connected_at_ms = now_ms;
		rec.last_seen_ms = now_ms;

		records_[session_id] = std::move(rec);
	}

	/// Returns false if the session is gone.
	bool mark_registered(const std::string &session_id,
						 const std::string &worker_id, const std::string &kind,
						 uint64_t capacity_cells, std::string summary,
						 uint64_t now_ms){
		std::lock_guard<std::mutex> lock{mutex_};

		auto iter = records_.find(session_id);
		if(iter == records_.end()){
			return false;
		}

		auto &rec = iter->second;
		rec.worker_id = worker_id;
		rec.kind = kind;
		rec.capacity_cells = capacity_cells;
		rec.summary = std::move(summary);
		rec.state = worker_state::registered;
		rec.registered_at_ms = now_ms;
		rec.last_seen_ms = now_ms;

		return true;
	}

	bool mark_ready(const std::string &session_id, data<sch::WorkerLive> live,
					uint64_t now_ms){
		std::lock_guard<std::mutex> lock{mutex_};

		auto iter = records_.find(session_id);
		if(iter == records_.end()){
			return false;
		}

		iter->second.state = worker_state::ready;
		iter->second.live = std::move(live);
		iter->second.last_seen_ms = now_ms;

		return true;
	}

	bool update_live(const std::string &session_id, data<sch::WorkerLive> live,
					 uint64_t now_ms){
		std::lock_guard<std::mutex> lock{mutex_};

		auto iter = records_.find(session_id);
		if(iter == records_.end()){
			return false;
		}

		iter->second.live = std::move(live);
		iter->second.last_seen_ms = now_ms;

		return true;
	}

	/**
	 * Records a pong. `echoed_sent_at_ms` is the orchestrator's own timestamp
	 * coming back, so round trip needs no agreement between the two clocks.
	 * Ignores the liveness effect of a pong whose sequence is not current.
	 */
	bool record_pong(const std::string &session_id, uint64_t seq,
					 uint64_t echoed_sent_at_ms, data<sch::WorkerLive> live,
					 uint64_t now_ms){
		std::lock_guard<std::mutex> lock{mutex_};

		auto iter = records_.find(session_id);
		if(iter == records_.end()){
			return false;
		}

		auto &rec = iter->second;

		if(seq != rec.ping_seq){
			// A pong for a ping we have already given up on. Counting it would
			// paper over a worker that is consistently one beat behind, so the
			// liveness side is left alone; the message still proves the channel
			// works, which is worth recording.
			rec.last_seen_ms = now_ms;
			return true;
		}

		rec.outstanding_pings = 0u;
		++rec.pongs;
		rec.last_seen_ms = now_ms;
		rec.live = std::move(live);

		// Guard against a clock that went backwards rather than producing a
		// negative round trip.
		const double rtt = (now_ms > echoed_sent_at_ms)
			? static_cast<double>(now_ms - echoed_sent_at_ms)
			: 0.0;

		if(rec.rtt_ewma_ms < 0.0){
			rec.rtt_ewma_ms = rtt;
		} else {
			// Smoothing factor chosen to react within a few beats without
			// letting one slow sample dominate.
			constexpr double alpha = 0.25;
			rec.rtt_ewma_ms = (alpha * rtt) + ((1.0 - alpha) * rec.rtt_ewma_ms);
		}

		// A worker that answers again stops being doubtful. It goes back to
		// registered rather than ready, because readiness is something only the
		// worker can assert.
		if(rec.state == worker_state::stale){
			rec.state = worker_state::registered;
		}

		return true;
	}

	/// Removes a session. Returns the record if there was one.
	std::optional<worker_record> remove(const std::string &session_id){
		std::lock_guard<std::mutex> lock{mutex_};

		auto iter = records_.find(session_id);
		if(iter == records_.end()){
			return std::nullopt;
		}

		worker_record rec = std::move(iter->second);
		records_.erase(iter);

		return rec;
	}

	/**
	 * One heartbeat tick.
	 *
	 * Bumps the ping sequence and the miss counter for every session, applies
	 * the stale and drop thresholds, and returns what to do. Nothing is sent
	 * from here -- the registry has nothing to send on, and in the orchestrator
	 * sending runs callbacks, which while holding this lock is how it would
	 * deadlock.
	 */
	struct tick_result {
		struct ping_target {
			std::string session_id;
			uint64_t seq;
		};

		std::vector<ping_target> to_ping;
		/// Sessions that passed drop_after; already removed from the registry.
		std::vector<worker_record> dropped;
		/// Sessions that just crossed into stale, for logging.
		std::vector<std::string> newly_stale;
	};

	tick_result tick(uint64_t now_ms){
		(void) now_ms;

		tick_result out;

		std::lock_guard<std::mutex> lock{mutex_};

		for(auto iter = records_.begin(); iter != records_.end(); ){
			auto &rec = iter->second;

			// A session still negotiating has no channel to ping on. It is not
			// unhealthy, it is just not up yet, so the thresholds do not apply.
			if(rec.state == worker_state::signalling){
				++iter;
				continue;
			}

			if(rec.outstanding_pings >= limits_.drop_after){
				out.dropped.push_back(rec);
				iter = records_.erase(iter);
				continue;
			}

			if(rec.outstanding_pings >= limits_.stale_after and
			   rec.state != worker_state::stale){
				rec.state = worker_state::stale;
				out.newly_stale.push_back(rec.session_id);
			}

			++rec.ping_seq;
			++rec.outstanding_pings;

			out.to_ping.push_back(
				tick_result::ping_target{rec.session_id, rec.ping_seq});

			++iter;
		}

		return out;
	}

	std::size_t size() const {
		std::lock_guard<std::mutex> lock{mutex_};
		return records_.size();
	}

	/// Snapshot for display. Copies, so the caller holds no lock.
	std::vector<worker_record> snapshot() const {
		std::lock_guard<std::mutex> lock{mutex_};

		std::vector<worker_record> out;
		out.reserve(records_.size());
		for(const auto &entry : records_){
			out.push_back(entry.second);
		}

		return out;
	}

private:
	mutable std::mutex mutex_;
	std::map<std::string, worker_record> records_;
	worker_thresholds limits_;
};

namespace impl {

inline std::string human_cells(uint64_t cells){
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(1);

	if(cells >= 1000000000ull){
		oss << (static_cast<double>(cells) / 1e9) << "G";
	} else if(cells >= 1000000ull){
		oss << (static_cast<double>(cells) / 1e6) << "M";
	} else if(cells >= 1000ull){
		oss << (static_cast<double>(cells) / 1e3) << "k";
	} else {
		oss << cells;
	}

	return oss.str();
}

} // namespace impl

/// Renders a snapshot as a fixed width table.
inline std::string render_table(const std::vector<worker_record> &records,
								uint64_t now_ms){
	std::ostringstream oss;

	oss << "  " << std::left
		<< std::setw(6)  << "sess"
		<< std::setw(18) << "worker"
		<< std::setw(9)  << "kind"
		<< std::setw(12) << "state"
		<< std::setw(9)  << "rtt"
		<< std::setw(8)  << "pongs"
		<< std::setw(8)  << "seen"
		<< std::setw(8)  << "cells"
		<< "detail" << "\n";

	for(const auto &rec : records){
		std::ostringstream rtt;
		if(rec.rtt_ewma_ms < 0.0){
			rtt << "-";
		} else {
			rtt << std::fixed << std::setprecision(1) << rec.rtt_ewma_ms << "ms";
		}

		std::ostringstream seen;
		const uint64_t age = (now_ms > rec.last_seen_ms)
			? (now_ms - rec.last_seen_ms) : 0u;
		seen << (age / 1000u) << "s";

		oss << "  " << std::left
			<< std::setw(6)  << rec.session_id
			<< std::setw(18) << (rec.worker_id.empty() ? "-" : rec.worker_id)
			<< std::setw(9)  << (rec.kind.empty() ? "-" : rec.kind)
			<< std::setw(12) << to_string(rec.state)
			<< std::setw(9)  << rtt.str()
			<< std::setw(8)  << rec.pongs
			<< std::setw(8)  << seen.str()
			<< std::setw(8)  << impl::human_cells(rec.capacity_cells)
			<< rec.summary << "\n";
	}

	if(records.empty()){
		oss << "  (no workers)\n";
	}

	return oss.str();
}

} // namespace swarm
} // namespace lbm
} // namespace kel
