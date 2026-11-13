#pragma once

#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

#include <GLES3/gl3.h>

#include <string>

/**
 * Owning a WebGL2 context.
 *
 * The probe deliberately runs on its own throwaway context rather than the one
 * the simulation will use. Measuring capacity means allocating until something
 * breaks, and when a browser hits GPU exhaustion it frequently answers by
 * killing the context rather than returning an error. Losing a context created
 * for the purpose costs nothing; losing the one holding the lattice costs the
 * run.
 */
namespace kel {
namespace lbm {
namespace webgl {

class context {
public:
	context() = default;

	context(const context &) = delete;
	context &operator=(const context &) = delete;

	~context(){ destroy(); }

	/**
	 * `target` is a CSS selector for the canvas, e.g. "#probe". In a worker
	 * this needs an OffscreenCanvas transferred from the main thread and the
	 * build flag to match; see the module's SConstruct.
	 */
	bool create(const std::string &target){
		destroy();

		EmscriptenWebGLContextAttributes attrs;
		emscripten_webgl_init_context_attributes(&attrs);

		attrs.majorVersion = 2;
		attrs.minorVersion = 0;

		// None of this is drawn to. Turning the extras off keeps the context
		// cheap and avoids the compositor holding buffers we never read.
		attrs.alpha = EM_FALSE;
		attrs.depth = EM_FALSE;
		attrs.stencil = EM_FALSE;
		attrs.antialias = EM_FALSE;

		// Accept a software context rather than refusing one. A modest device
		// should still be able to join: what it reports here is a memory
		// budget, and that budget is real even when the renderer behind it is
		// slow. The cost is that capacity alone cannot distinguish a software
		// context from a fast one -- that is a throughput question, which is
		// what the perf section of the capability is reserved for.
		attrs.failIfMajorPerformanceCaveat = EM_FALSE;

		handle_ = emscripten_webgl_create_context(target.c_str(), &attrs);
		if(handle_ <= 0){
			handle_ = 0;
			return false;
		}

		if(emscripten_webgl_make_context_current(handle_) != EMSCRIPTEN_RESULT_SUCCESS){
			destroy();
			return false;
		}

		return true;
	}

	bool valid() const { return handle_ > 0; }

	bool lost() const {
		return handle_ > 0 and emscripten_is_webgl_context_lost(handle_);
	}

	/**
	 * Extensions have to be asked for by name before the entry points work,
	 * even where the underlying GLES symbol already exists.
	 */
	bool enable_extension(const char *name){
		if(handle_ <= 0){
			return false;
		}
		return emscripten_webgl_enable_extension(handle_, name);
	}

	bool make_current(){
		return handle_ > 0 and
			emscripten_webgl_make_context_current(handle_)
				== EMSCRIPTEN_RESULT_SUCCESS;
	}

	void destroy(){
		if(handle_ > 0){
			emscripten_webgl_destroy_context(handle_);
			handle_ = 0;
		}
	}

private:
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle_ = 0;
};

} // namespace webgl
} // namespace lbm
} // namespace kel
