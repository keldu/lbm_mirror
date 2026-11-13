#pragma once

#include <cstdint>

/**
 * Finding out how much a device will actually give you.
 *
 * WebGL2 reports limits but not capacity. MAX_TEXTURE_SIZE on a typical desktop
 * implies something like 25 GB of lattice, roughly a hundred times what the GPU
 * will really hand over, and there is no query for the real figure --
 * navigator.deviceMemory is system RAM and unrelated. So capacity is measured:
 * try an allocation, see whether it survives, adjust.
 *
 * The strategy lives here rather than next to the GL calls because it is
 * ordinary arithmetic and deserves to be tested as such. The caller owns the
 * part that cannot be tested without a browser -- actually allocating -- and
 * feeds the answers back in:
 *
 *     allocation_search search{cfg};
 *     while(not search.done()){
 *         search.report(try_allocate(search.candidate()));
 *     }
 *     use(search.best_cells());
 *
 * Grow by doubling until something fails, then bisect. Doubling finds the right
 * order of magnitude in a handful of steps even when devices differ by a factor
 * of a thousand; bisecting then tightens it without the linear crawl a pure
 * step search would need.
 */
namespace kel {
namespace lbm {
namespace swarm {

class allocation_search {
public:
	struct config {
		/**
		 * First attempt. Deliberately small: the opening trial should succeed
		 * on anything that can run at all, because a failure here means the
		 * device is unusable rather than merely modest.
		 */
		uint64_t start_cells = 65536u;

		/**
		 * Hard ceiling from the texture dimension, in cells. Zero means no
		 * ceiling. Reaching it is a different answer from running out of
		 * memory and is reported separately.
		 */
		uint64_t ceiling_cells = 0u;

		/// Bisection steps once a failure has bracketed the answer. Four gets
		/// within about a sixteenth of the bracket, which is finer than the
		/// figure deserves given it is a moving target anyway.
		uint64_t bisect_steps = 4u;

		/// Backstop so a device that answers inconsistently cannot spin.
		uint64_t max_trials = 32u;
	};

	explicit allocation_search(config cfg) : cfg_{cfg} {
		candidate_ = cfg_.start_cells;

		if(cfg_.ceiling_cells > 0u and candidate_ > cfg_.ceiling_cells){
			candidate_ = cfg_.ceiling_cells;
		}

		if(candidate_ == 0u){
			phase_ = phase::finished;
		}
	}

	bool done() const { return phase_ == phase::finished; }

	/// The cell count to attempt next. Meaningless once done().
	uint64_t candidate() const { return candidate_; }

	/// Largest allocation that survived. Zero means nothing did.
	uint64_t best_cells() const { return best_; }

	uint64_t trials() const { return trials_; }

	/**
	 * True when the search stopped because the texture dimension ran out
	 * rather than because memory did. Worth distinguishing: one says the
	 * device is small, the other says it is oddly shaped.
	 */
	bool hit_ceiling() const { return hit_ceiling_; }

	void report(bool ok){
		if(phase_ == phase::finished){
			return;
		}

		++trials_;

		if(phase_ == phase::grow){
			report_grow(ok);
		} else {
			report_bisect(ok);
		}

		if(trials_ >= cfg_.max_trials){
			phase_ = phase::finished;
		}
	}

private:
	enum class phase { grow, bisect, finished };

	void report_grow(bool ok){
		if(not ok){
			failed_at_ = candidate_;

			if(best_ == 0u){
				// Even the opening attempt failed. There is nothing to
				// bisect between, and the honest answer is zero.
				phase_ = phase::finished;
				return;
			}

			phase_ = phase::bisect;
			advance_bisect();
			return;
		}

		best_ = candidate_;

		if(cfg_.ceiling_cells > 0u and candidate_ >= cfg_.ceiling_cells){
			// The texture dimension, not memory, is what stopped us.
			hit_ceiling_ = true;
			phase_ = phase::finished;
			return;
		}

		uint64_t next = candidate_ * 2u;
		if(next < candidate_){
			// Overflow. Nothing sane gets here, but silently wrapping to a
			// tiny candidate would turn a huge device into a small one.
			phase_ = phase::finished;
			return;
		}
		if(cfg_.ceiling_cells > 0u and next > cfg_.ceiling_cells){
			next = cfg_.ceiling_cells;
		}
		if(next == candidate_){
			phase_ = phase::finished;
			return;
		}

		candidate_ = next;
	}

	void report_bisect(bool ok){
		if(ok){
			best_ = candidate_;
		} else {
			failed_at_ = candidate_;
		}
		advance_bisect();
	}

	void advance_bisect(){
		if(bisects_done_ >= cfg_.bisect_steps){
			phase_ = phase::finished;
			return;
		}
		if(failed_at_ <= best_ + 1u){
			// Bracket closed; there is nothing left between them.
			phase_ = phase::finished;
			return;
		}

		const uint64_t mid = best_ + ((failed_at_ - best_) / 2u);
		if(mid == best_ or mid == failed_at_){
			phase_ = phase::finished;
			return;
		}

		candidate_ = mid;
		++bisects_done_;
	}

	config cfg_;

	phase phase_ = phase::grow;
	uint64_t candidate_ = 0u;
	/// Largest known good.
	uint64_t best_ = 0u;
	/// Smallest known bad.
	uint64_t failed_at_ = 0u;
	uint64_t trials_ = 0u;
	uint64_t bisects_done_ = 0u;
	bool hit_ceiling_ = false;
};

} // namespace swarm
} // namespace lbm
} // namespace kel
