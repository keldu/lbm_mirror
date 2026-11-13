#pragma once

#include "context.hpp"

#include <kel/lbm/abstract/data.hpp>
#include <kel/lbm/swarm/capability.hpp>
#include <kel/lbm/swarm/capacity.hpp>
#include <kel/lbm/swarm/probe_search.hpp>

#include <cstdint>
#include <string>
#include <vector>

/**
 * Measuring what a browser will actually give you.
 *
 * Split in two on purpose. The limits and extensions are a handful of queries;
 * the budget is a measurement, because WebGL2 has no capacity query and the
 * limits it does report overstate the truth by around a hundredfold on a
 * typical desktop.
 *
 * The strategy for that measurement is not here -- it is swarm's
 * allocation_search, which is ordinary arithmetic and is unit tested. What
 * lives here is only the part that genuinely needs a GPU: allocate, check,
 * destroy. Everything in this header is unreachable without a browser, so the
 * rule is that it stays thin enough to audit by reading.
 *
 * Header only, like core and swarm, so a wasm consumer needs no link wiring --
 * which matters because em++ cannot consume nix buildInputs the way the native
 * builds can.
 */
namespace kel {
namespace lbm {
namespace webgl {

/// How a single allocation attempt ended. Not a bool, because "the context
/// died" has to be distinguished from "that was too big".
enum class trial_result {
	ok,
	/// Rejected, by a GL error or an incomplete framebuffer.
	too_big,
	/// The context went away. Everything after this is meaningless.
	context_lost,
};

struct probe_config {
	/// CSS selector for the throwaway canvas.
	std::string canvas = "#kel-probe";

	/// Distribution planes to allocate per attempt. Three is D2Q9.
	uint32_t planes = 3u;

	/// Both ping pong copies, since the real thing needs both resident.
	uint32_t states = 2u;

	/**
	 * Leaves room for the canvas, the compositor and other tabs. A guess,
	 * and flagged as one: it wants measuring against a real run on real
	 * hardware before it is trusted.
	 */
	float safety_factor = 0.6f;
};

namespace impl {

inline uint32_t get_int(GLenum name){
	GLint value = 0;
	glGetIntegerv(name, &value);
	return (value > 0) ? static_cast<uint32_t>(value) : 0u;
}

/// Clears any error already pending, so the next check reports our own.
inline void drain_errors(){
	for(int i = 0; i < 16; ++i){
		if(glGetError() == GL_NO_ERROR){
			return;
		}
	}
}

} // namespace impl

/// Fills the queryable limits. Requires a current context.
inline void query_limits(data<sch::GlLimits> &out){
	out.get<"max_texture_size">().set(impl::get_int(GL_MAX_TEXTURE_SIZE));
	out.get<"max_3d_texture_size">().set(impl::get_int(GL_MAX_3D_TEXTURE_SIZE));
	out.get<"max_array_texture_layers">().set(
		impl::get_int(GL_MAX_ARRAY_TEXTURE_LAYERS));
	out.get<"max_draw_buffers">().set(impl::get_int(GL_MAX_DRAW_BUFFERS));
	out.get<"max_color_attachments">().set(impl::get_int(GL_MAX_COLOR_ATTACHMENTS));
	out.get<"max_texture_image_units">().set(
		impl::get_int(GL_MAX_TEXTURE_IMAGE_UNITS));
	out.get<"max_combined_texture_image_units">().set(
		impl::get_int(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
	out.get<"max_renderbuffer_size">().set(impl::get_int(GL_MAX_RENDERBUFFER_SIZE));
	out.get<"max_fragment_uniform_vectors">().set(
		impl::get_int(GL_MAX_FRAGMENT_UNIFORM_VECTORS));

	GLint dims[2] = {0, 0};
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
	out.get<"max_viewport_dims">().at({{0u}}).set(
		(dims[0] > 0) ? static_cast<uint32_t>(dims[0]) : 0u);
	out.get<"max_viewport_dims">().at({{1u}}).set(
		(dims[1] > 0) ? static_cast<uint32_t>(dims[1]) : 0u);
}

/// Fills the extension and context properties. Requires a current context.
inline void query_features(context &ctx, data<sch::GlFeatures> &out){
	out.get<"webgl2">().set(ctx.valid());

	// Whether this is running off an OffscreenCanvas, which is what a worker
	// thread needs in order to have a context at all.
#if defined(__EMSCRIPTEN_PTHREADS__)
	out.get<"offscreen_canvas">().set(true);
#else
	out.get<"offscreen_canvas">().set(false);
#endif

	// The hard gate. Without it, float textures are not colour renderable.
	out.get<"color_buffer_float">().set(
		ctx.enable_extension("EXT_color_buffer_float"));
	out.get<"color_buffer_half_float">().set(
		ctx.enable_extension("EXT_color_buffer_half_float"));

	// Not needed by the solver, which fetches by texel rather than sampling.
	// Reported because a smooth visualisation would want it.
	out.get<"texture_float_linear">().set(
		ctx.enable_extension("OES_texture_float_linear"));

	// Will matter for immersed boundary force spreading, which accumulates by
	// blending. Nothing uses it yet.
	out.get<"float_blend">().set(ctx.enable_extension("EXT_float_blend"));

	out.get<"parallel_shader_compile">().set(
		ctx.enable_extension("KHR_parallel_shader_compile"));
	out.get<"disjoint_timer_query">().set(
		ctx.enable_extension("EXT_disjoint_timer_query_webgl2"));
}

/**
 * One allocation attempt at `nx` by `ny`.
 *
 * Does the full sequence rather than just asking for memory: immutable storage,
 * framebuffer completeness, and then a real clear plus a one texel readback.
 * The last part is not ceremony -- a driver is entitled to defer the actual
 * allocation until first use, so an attempt that stops at glTexStorage2D can
 * report success against memory that does not exist.
 */
inline trial_result try_allocate(context &ctx, const probe_config &cfg,
								 uint32_t nx, uint32_t ny){
	if(nx == 0u or ny == 0u){
		return trial_result::too_big;
	}

	impl::drain_errors();

	const uint32_t count = cfg.planes * cfg.states;

	std::vector<GLuint> textures(count, 0u);
	GLuint fbo = 0u;

	// Every exit path goes through here. A probe that leaks on failure would
	// poison the attempts that follow it, which is precisely the regime where
	// failures are expected.
	auto cleanup = [&](){
		if(fbo != 0u){
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
		}
		for(GLuint tex : textures){
			if(tex != 0u){
				glDeleteTextures(1, &tex);
			}
		}
		glFlush();
	};

	glGenTextures(static_cast<GLsizei>(count), textures.data());

	for(uint32_t i = 0u; i < count; ++i){
		glBindTexture(GL_TEXTURE_2D, textures[i]);

		// Immutable storage. The mutable glTexImage2D path lets a driver defer
		// the decision, so it can report success for memory it has not
		// committed to.
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F,
					   static_cast<GLsizei>(nx), static_cast<GLsizei>(ny));

		if(glGetError() != GL_NO_ERROR){
			cleanup();
			return ctx.lost() ? trial_result::context_lost : trial_result::too_big;
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	// Attach one state's worth, which is what a real pass renders into.
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	std::vector<GLenum> attachments;
	attachments.reserve(cfg.planes);
	for(uint32_t i = 0u; i < cfg.planes; ++i){
		glFramebufferTexture2D(GL_FRAMEBUFFER,
							   GL_COLOR_ATTACHMENT0 + i,
							   GL_TEXTURE_2D, textures[i], 0);
		attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
	}
	glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		cleanup();
		return ctx.lost() ? trial_result::context_lost : trial_result::too_big;
	}

	// Force the driver to commit. Clearing touches every attachment, and the
	// readback blocks until it has actually happened -- without this the whole
	// sequence can succeed against memory that was never really allocated.
	glViewport(0, 0, static_cast<GLsizei>(nx), static_cast<GLsizei>(ny));
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	float pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);

	const GLenum err = glGetError();
	const bool was_lost = ctx.lost();

	cleanup();

	if(was_lost){
		return trial_result::context_lost;
	}
	if(err != GL_NO_ERROR){
		return trial_result::too_big;
	}

	return trial_result::ok;
}

/**
 * Runs the whole probe on a throwaway context and fills the capability.
 *
 * Returns false if a WebGL2 context could not be had at all, or if
 * EXT_color_buffer_float is missing -- without it a float texture is not
 * colour renderable and there is no solver on this device, so there is nothing
 * worth measuring. The capability is still filled in either case: an unusable
 * worker is better reported than silent.
 */
inline bool probe_webgl2(const probe_config &cfg, data<sch::WebGl2Capability> &out){
	context ctx;
	if(not ctx.create(cfg.canvas)){
		return false;
	}

	query_limits(out.get<"limits">());
	query_features(ctx, out.get<"features">());

	if(not out.get<"features">().get<"color_buffer_float">().get()){
		return false;
	}

	const uint64_t max_tex = out.get<"limits">().get<"max_texture_size">().get();

	const uint64_t bytes_per_cell =
		swarm::packing_math::bytes_per_cell(9u, cfg.states, 1u, true);

	swarm::allocation_search::config scfg;
	scfg.ceiling_cells =
		swarm::capacity_math::texture_bound_cells_2d(max_tex, 0u);

	swarm::allocation_search search{scfg};

	bool context_lost = false;
	uint32_t trials = 0u;

	while(not search.done()){
		// Square-ish, so neither edge runs into max_texture_size before the
		// area runs into memory.
		const auto dom = swarm::capacity_math::fit_domain_2d(
			search.candidate(), 1u, 1u, max_tex, 0u);

		const trial_result result = try_allocate(
			ctx, cfg, static_cast<uint32_t>(dom.nx),
			static_cast<uint32_t>(dom.ny));

		++trials;

		if(result == trial_result::context_lost){
			// Browsers often answer GPU exhaustion by killing the context
			// rather than returning an error. Everything after this would be
			// meaningless, so stop and keep what was already measured.
			context_lost = true;
			break;
		}

		search.report(result == trial_result::ok);
	}

	const uint64_t best_bytes = search.best_cells() * bytes_per_cell;

	auto &budget = out.get<"budget">();
	budget.get<"probed_bytes_max">().set(best_bytes);
	budget.get<"probed_bytes_safe">().set(
		static_cast<uint64_t>(static_cast<double>(best_bytes) *
							  static_cast<double>(cfg.safety_factor)));
	budget.get<"probe_trials">().set(trials);
	budget.get<"context_lost_during_probe">().set(context_lost);
	budget.get<"probe_hit_texture_size_bound">().set(search.hit_ceiling());
	budget.get<"safety_factor">().set(cfg.safety_factor);

	return not context_lost and search.best_cells() > 0u;
}

} // namespace webgl
} // namespace lbm
} // namespace kel
