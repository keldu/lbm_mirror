#include <forstio/test/suite.hpp>

#include "../c++/capacity.hpp"
#include "../c++/probe_search.hpp"

#include <string>

namespace {
using namespace kel::lbm::swarm;

/**
 * Runs a search against a simulated device whose real capacity is `truth`,
 * returning the outcome. This is the whole point of keeping the strategy apart
 * from the GL calls: a device that would take a browser and several seconds to
 * interrogate is a predicate here.
 */
struct probe_outcome {
	uint64_t best;
	uint64_t trials;
	bool hit_ceiling;
};

probe_outcome run_against(uint64_t truth, allocation_search::config cfg = {}){
	allocation_search search{cfg};

	while(not search.done()){
		search.report(search.candidate() <= truth);
	}

	return probe_outcome{search.best_cells(), search.trials(),
						 search.hit_ceiling()};
}

SAW_TEST("Probe / never reports more than the device can do"){
	// The property that matters most. Over-reporting means the orchestrator
	// hands out a domain that does not fit, and the worker fails at the point
	// where it is least convenient.
	const uint64_t truths[] = {
		65536u, 100000u, 1000000u, 2097152u, 5600000u, 12345678u,
		99999999u, 1u << 28,
	};

	for(uint64_t truth : truths){
		const auto out = run_against(truth);
		SAW_EXPECT(out.best <= truth,
			"A probe must never claim more than the device allows");
	}
}

SAW_TEST("Probe / gets close to the real capacity"){
	// Within a quarter is plenty: the figure is a planning estimate against a
	// budget that shifts as other tabs come and go.
	const uint64_t truths[] = {
		200000u, 1000000u, 3000000u, 5600000u, 22000000u,
	};

	for(uint64_t truth : truths){
		const auto out = run_against(truth);
		SAW_EXPECT(out.best * 4u >= truth * 3u,
			std::string{"Probe landed too far below the truth for "}
				+ std::to_string(truth));
	}
}

SAW_TEST("Probe / converges in a handful of trials"){
	// Doubling then bisecting should cost a dozen or so attempts even across
	// three orders of magnitude of device.
	allocation_search::config cfg;

	for(uint64_t truth = 65536u; truth < (1ull << 30); truth *= 3u){
		const auto out = run_against(truth, cfg);
		SAW_EXPECT(out.trials <= cfg.max_trials,
			"The backstop must hold");
		SAW_EXPECT(out.trials <= 20u,
			std::string{"Too many trials for truth "} + std::to_string(truth));
	}
}

SAW_TEST("Probe / a device that cannot do the opening attempt reports zero"){
	allocation_search::config cfg;
	cfg.start_cells = 65536u;

	const auto out = run_against(1000u, cfg);
	SAW_EXPECT(out.best == 0u,
		"Failing the smallest attempt must report no capacity, not a guess");
	SAW_EXPECT(out.trials == 1u, "and must not keep trying");
}

SAW_TEST("Probe / the texture ceiling is reported, not confused with memory"){
	allocation_search::config cfg;
	cfg.ceiling_cells = 1000000u;

	// Memory is effectively unlimited here, so the ceiling is what stops it.
	const auto out = run_against(1ull << 40, cfg);
	SAW_EXPECT(out.best == cfg.ceiling_cells, "It reaches the ceiling exactly");
	SAW_EXPECT(out.hit_ceiling,
		"and says the texture dimension stopped it, not memory");

	// Whereas a device that runs out of memory first must not claim the
	// ceiling stopped it -- those are different things to report upstream.
	const auto mem = run_against(300000u, cfg);
	SAW_EXPECT(not mem.hit_ceiling,
		"Running out of memory below the ceiling is not a ceiling hit");
	SAW_EXPECT(mem.best <= 300000u, "and still must not over-report");
}

SAW_TEST("Probe / a ceiling below the opening attempt is clamped"){
	allocation_search::config cfg;
	cfg.start_cells = 65536u;
	cfg.ceiling_cells = 1024u;

	allocation_search search{cfg};
	SAW_EXPECT(search.candidate() == 1024u,
		"The first attempt must not exceed the ceiling");

	const auto out = run_against(1ull << 30, cfg);
	SAW_EXPECT(out.best == 1024u, "and the answer is the ceiling");
	SAW_EXPECT(out.hit_ceiling, "reported as such");
}

SAW_TEST("Probe / a zero ceiling means no capacity at all"){
	allocation_search::config cfg;
	cfg.ceiling_cells = 0u;
	cfg.start_cells = 0u;

	allocation_search search{cfg};
	SAW_EXPECT(search.done(), "Nothing to try means the search is already over");
	SAW_EXPECT(search.best_cells() == 0u, "and the answer is zero");
}

SAW_TEST("Probe / reporting after completion is ignored"){
	allocation_search search{allocation_search::config{}};
	while(not search.done()){
		search.report(false);
	}

	const uint64_t trials = search.trials();
	search.report(true);
	SAW_EXPECT(search.trials() == trials,
		"A late answer must not restart a finished search");
	SAW_EXPECT(search.best_cells() == 0u, "nor change the result");
}

/**
 * A device that answers inconsistently -- which a real one can, since memory
 * pressure moves while you are measuring -- must still terminate.
 */
SAW_TEST("Probe / an unreliable device still terminates"){
	allocation_search::config cfg;

	// Alternating yes/no regardless of size.
	{
		allocation_search search{cfg};
		bool flip = true;
		uint64_t guard = 0u;
		while(not search.done()){
			search.report(flip);
			flip = not flip;
			SAW_EXPECT(++guard < 1000u, "The search must terminate");
		}
		SAW_EXPECT(search.trials() <= cfg.max_trials, "within the backstop");
	}

	// Always yes, unbounded device: the doubling has to stop somewhere.
	{
		allocation_search search{cfg};
		uint64_t guard = 0u;
		while(not search.done()){
			search.report(true);
			SAW_EXPECT(++guard < 1000u, "Unbounded success must still terminate");
		}
		SAW_EXPECT(search.trials() <= cfg.max_trials, "within the backstop");
	}
}

/**
 * The probe answers in cells; the orchestrator wants a domain. Checking the
 * two compose means the number that comes off a device is usable as it stands.
 */
SAW_TEST("Probe / the measured capacity feeds the domain fitting"){
	constexpr uint64_t bpc = packing_math::bytes_per_cell(9u, 2u, 1u, true);
	const uint64_t max_tex = 16384u;

	// A device good for about 512MB of lattice.
	const uint64_t truth = capacity_math::cells_from_bytes(
		512ull * 1024ull * 1024ull, bpc);

	allocation_search::config cfg;
	cfg.ceiling_cells = capacity_math::texture_bound_cells_2d(max_tex, 0u);

	const auto out = run_against(truth, cfg);
	SAW_EXPECT(out.best <= truth, "Measured capacity must not exceed the truth");
	SAW_EXPECT(not out.hit_ceiling,
		"Memory binds well before the texture dimension on a device like this");

	const auto dom = capacity_math::fit_domain_2d(out.best, 4u, 1u, max_tex, 0u);
	SAW_EXPECT(dom.cells() <= out.best,
		"The fitted domain must fit what was actually measured");
	SAW_EXPECT(dom.cells() * bpc <= 512ull * 1024ull * 1024ull,
		"and therefore fit in the memory the device really had");
}
}
