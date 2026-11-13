#pragma once

#include <kel/lbm/abstract/schema.hpp>

/**
 * What a worker tells the orchestrator about itself.
 *
 * The envelope is deliberately backend agnostic: common fields, then one
 * section named by "kind". Browsers running WebGL2 and native processes are
 * both first class worker kinds, and the swarm is meant to take WebGPU runners
 * later; a string tag survives version skew where a positional format does not.
 *
 * A worker of another kind carries its own section instead. Nothing has to
 * carry a section it has no business describing, because each kind gets its own
 * full capability type -- see CapabilityOf below and the hello variants in
 * message.hpp.
 */
namespace kel {
namespace lbm {
namespace sch {

/**
 * Queryable WebGL2 limits, straight from glGetIntegerv.
 *
 * Worth knowing which of these actually bind. max_draw_buffers and
 * max_color_attachments are guaranteed to be only 4, and since a D2Q9 lattice
 * needs ceil(9/4) = 3 render targets they are the limits that decide whether a
 * descriptor fits in a single pass at all. The rest are either generous in
 * practice or bound by memory long before they bind by count.
 */
using GlLimits = Struct<
	Member<UInt32, "max_texture_size">,
	Member<UInt32, "max_3d_texture_size">,
	Member<UInt32, "max_array_texture_layers">,
	Member<UInt32, "max_draw_buffers">,
	Member<UInt32, "max_color_attachments">,
	Member<UInt32, "max_texture_image_units">,
	Member<UInt32, "max_combined_texture_image_units">,
	Member<UInt32, "max_renderbuffer_size">,
	Member<UInt32, "max_fragment_uniform_vectors">,
	Member<FixedArray<UInt32, 2u>, "max_viewport_dims">
>;

/**
 * Extension and context properties.
 *
 * color_buffer_float is a hard gate: without EXT_color_buffer_float a float
 * texture is not color renderable and there is no f32 solver on this device at
 * all. The others are reported so the orchestrator can decide, not so the
 * worker can.
 */
using GlFeatures = Struct<
	Member<Bool, "webgl2">,
	Member<Bool, "offscreen_canvas">,
	Member<Bool, "color_buffer_float">,
	Member<Bool, "color_buffer_half_float">,
	Member<Bool, "texture_float_linear">,
	Member<Bool, "float_blend">,
	Member<Bool, "parallel_shader_compile">,
	Member<Bool, "disjoint_timer_query">
>;

/**
 * What the worker measured, as opposed to what it was told.
 *
 * There is no queryable "available VRAM" in WebGL2 -- navigator.deviceMemory is
 * system RAM and unrelated -- and max_texture_size overstates capacity by
 * roughly a hundredfold on a typical desktop. So these come from an actual
 * allocation trial.
 *
 * probed_bytes_max is the largest allocation that survived; probed_bytes_safe
 * applies safety_factor to leave room for the canvas, the compositor and other
 * tabs. Both are reported so the orchestrator can apply its own policy rather
 * than inheriting the worker's.
 *
 * probe_hit_texture_size_bound distinguishes "ran out of memory" from "ran into
 * max_texture_size", which are very different signals about a device.
 */
using GlBudget = Struct<
	Member<UInt64, "probed_bytes_max">,
	Member<UInt64, "probed_bytes_safe">,
	Member<UInt32, "probe_trials">,
	Member<Bool, "context_lost_during_probe">,
	Member<Bool, "probe_hit_texture_size_bound">,
	Member<Float32, "safety_factor">
>;

/**
 * One feasible way to lay a lattice out in textures on this device.
 *
 * This type has three jobs and keeping it as one type is what stops them
 * drifting: it is an element of the capability report, it is the assignment
 * message the orchestrator sends back, and it is the input to the shader
 * preamble generator.
 *
 * The slot tables are the indirection that makes pixel stuffing expressible --
 * slot_tex_of_q[i] / slot_cmp_of_q[i] say which texture and which channel
 * direction i lives in, and the aux tables do the same for the non
 * distribution fields packed into otherwise idle slots.
 */
using PackingPlan = Struct<
	Member<String, "layout">,
	Member<String, "aux_mode">,
	Member<UInt8, "d">,
	Member<UInt8, "q">,
	Member<UInt8, "ghost">,
	Member<UInt8, "storage_bits">,
	Member<UInt8, "slots_per_texel">,
	Member<UInt8, "textures_df">,
	Member<UInt8, "textures_aux">,
	Member<UInt8, "states">,
	Member<UInt8, "passes">,
	Member<UInt8, "idle_slots">,
	Member<FixedArray<UInt32, 3u>, "domain">,
	Member<FixedArray<UInt32, 2u>, "tiles">,
	Member<Bool, "tiles_pow2">,
	Member<FixedArray<UInt32, 2u>, "texture_size">,
	Member<UInt64, "bytes_total">,
	Member<UInt64, "bytes_useful">,
	Member<Array<UInt8, 1u>, "slot_tex_of_q">,
	Member<Array<UInt8, 1u>, "slot_cmp_of_q">,
	Member<Array<UInt8, 1u>, "slot_tex_of_aux">,
	Member<Array<UInt8, 1u>, "slot_cmp_of_aux">,
	Member<Array<String, 1u>, "aux_names">
>;

using WebGl2Capability = Struct<
	Member<GlLimits, "limits">,
	Member<GlFeatures, "features">,
	Member<GlBudget, "budget">,
	Member<Array<PackingPlan, 1u>, "feasible">
>;

/**
 * A worker that is a process rather than a browser tab.
 *
 * Native workers are not only a test fixture -- they are a worker kind in their
 * own right, and the orchestrator should be able to hand them real work. What
 * they report is correspondingly different: there is no texture budget to probe
 * because there are no textures, so the interesting numbers are threads and
 * addressable memory.
 */
using NativeCapability = Struct<
	Member<UInt32, "threads">,
	Member<UInt64, "memory_bytes">,
	Member<String, "cpu">,
	Member<Bool, "has_sycl">
>;

/**
 * The part every worker reports, whatever it is.
 *
 * This is deliberately the *whole* type rather than a prefix of a larger one.
 * Decoding runs strict, so a struct naming every backend section would oblige a
 * browser to describe a CPU and a native process to describe a texture budget.
 * Instead the common part stands alone and is enough to route on: decode this,
 * read "kind", then decode again as the matching full type below. The backend
 * section is skipped on the first pass because unknown keys always are.
 */
using WorkerCapability = Struct<
	Member<String, "worker_id">,
	Member<String, "kind">,
	Member<String, "schema_version">,
	Member<UInt64, "reported_at_ms">,
	Member<String, "user_agent">
>;

/**
 * The common part plus exactly one backend section. One alias per kind, so
 * adding a kind never widens what the existing ones have to fill in.
 */
template <typename Section, string_literal Key>
using CapabilityOf = Struct<
	Member<String, "worker_id">,
	Member<String, "kind">,
	Member<String, "schema_version">,
	Member<UInt64, "reported_at_ms">,
	Member<String, "user_agent">,
	Member<Section, Key>
>;

using WebGl2WorkerCapability = CapabilityOf<WebGl2Capability, "webgl2">;
using NativeWorkerCapability = CapabilityOf<NativeCapability, "native">;

/**
 * The small, frequently resent part: enough to monitor a worker without
 * re-sending its capability. Rides along on pong so a heartbeat doubles as a
 * status update.
 */
using WorkerLive = Struct<
	Member<String, "state">,
	Member<UInt64, "uptime_ms">,
	Member<UInt64, "steps_done">,
	Member<String, "last_error">
>;

} // namespace sch
} // namespace lbm
} // namespace kel
