#include <forstio/test/suite.hpp>

#include "../c++/wire.hpp"

#include <string>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;
using namespace kel::lbm::swarm;

/**
 * Builds a filled-in live block. Every member is set, because the wire codecs
 * run strict and a half built message is exactly what these tests are trying
 * to catch elsewhere.
 */
data<sch::WorkerLive> make_live(){
	data<sch::WorkerLive> live;
	live.get<"state">().set(std::string{wstate::ready});
	live.get<"uptime_ms">().set(1234u);
	live.get<"steps_done">().set(42u);
	live.get<"last_error">().set(std::string{""});
	return live;
}

SAW_TEST("Wire / pong round trips unchanged"){
	auto msg = make_message<sch::MsgPong>(msg::pong, "worker-7", 19u);
	msg.get<"sent_at_ms">().set(1000u);
	msg.get<"worker_now_ms">().set(2000u);
	msg.get<"live">() = make_live();

	auto eo_text = encode_message<sch::MsgPong>(msg);
	SAW_EXPECT(not eo_text.is_error(), "Encoding a complete message must succeed");
	const std::string text = eo_text.get_value();

	auto eo_back = decode_message<sch::MsgPong>(text);
	SAW_EXPECT(not eo_back.is_error(), "Decoding our own output must succeed");
	auto& back = eo_back.get_value();

	SAW_EXPECT(back.get<"type">().stl_view() == msg::pong, "type survives");
	SAW_EXPECT(back.get<"worker_id">().stl_view() == "worker-7", "worker_id survives");
	SAW_EXPECT(back.get<"seq">().get() == 19u, "seq survives");
	SAW_EXPECT(back.get<"sent_at_ms">().get() == 1000u, "echoed timestamp survives");
	SAW_EXPECT(back.get<"worker_now_ms">().get() == 2000u, "worker clock survives");
	SAW_EXPECT(back.get<"live">().get<"steps_done">().get() == 42u,
		"Nested struct members survive");
	SAW_EXPECT(back.get<"live">().get<"state">().stl_view() == wstate::ready,
		"Nested strings survive");
}

SAW_TEST("Wire / welcome and ping round trip"){
	auto wel = make_message<sch::MsgWelcome>(msg::welcome, "worker-1", 0u);
	wel.get<"heartbeat_ms">().set(2000u);
	wel.get<"schema_version">().set(std::string{schema_version});

	auto eo_text = encode_message<sch::MsgWelcome>(wel);
	SAW_EXPECT(not eo_text.is_error(), "welcome encodes");
	auto eo_back = decode_message<sch::MsgWelcome>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "welcome decodes");
	SAW_EXPECT(eo_back.get_value().get<"heartbeat_ms">().get() == 2000u,
		"heartbeat survives");

	auto png = make_message<sch::MsgPing>(msg::ping, "worker-1", 5u);
	png.get<"sent_at_ms">().set(999u);
	auto eo_ptext = encode_message<sch::MsgPing>(png);
	SAW_EXPECT(not eo_ptext.is_error(), "ping encodes");
	auto eo_pback = decode_message<sch::MsgPing>(eo_ptext.get_value());
	SAW_EXPECT(not eo_pback.is_error(), "ping decodes");
	SAW_EXPECT(eo_pback.get_value().get<"sent_at_ms">().get() == 999u,
		"ping timestamp survives");
}

/**
 * The two stage decode is the whole reason this protocol works without a
 * tagged union: reading the header out of a full message has to succeed for
 * every message type, skipping whatever payload it does not know about.
 */
SAW_TEST("Wire / the header can be read out of any message"){
	auto png = make_message<sch::MsgPing>(msg::ping, "w-a", 1u);
	png.get<"sent_at_ms">().set(7u);
	auto eo_png = encode_message<sch::MsgPing>(png);
	SAW_EXPECT(not eo_png.is_error(), "ping encodes");

	auto eo_hdr = peek_header(eo_png.get_value());
	SAW_EXPECT(not eo_hdr.is_error(),
		"Reading a header out of a ping must skip the payload, not fail");
	SAW_EXPECT(header_type(eo_hdr.get_value()) == msg::ping, "Header reports ping");
	SAW_EXPECT(header_worker_id(eo_hdr.get_value()) == "w-a", "Header reports the id");

	auto pong = make_message<sch::MsgPong>(msg::pong, "w-b", 2u);
	pong.get<"sent_at_ms">().set(1u);
	pong.get<"worker_now_ms">().set(2u);
	pong.get<"live">() = make_live();
	auto eo_pong = encode_message<sch::MsgPong>(pong);
	SAW_EXPECT(not eo_pong.is_error(), "pong encodes");

	auto eo_hdr2 = peek_header(eo_pong.get_value());
	SAW_EXPECT(not eo_hdr2.is_error(),
		"A nested struct payload must also be skippable");
	SAW_EXPECT(header_type(eo_hdr2.get_value()) == msg::pong, "Header reports pong");
	SAW_EXPECT(header_worker_id(eo_hdr2.get_value()) == "w-b", "Header reports the id");
}

/**
 * The failure this protocol must not have. codec_config<enc::Json>::strict
 * defaults to false so that config files can omit their defaults; on a wire
 * that would let a worker send "{}" and have it decode into a silently zeroed
 * registration.
 */
SAW_TEST("Wire / an empty object is rejected, not defaulted"){
	auto eo = decode_message<sch::MsgPing>("{}");
	SAW_EXPECT(eo.is_error(),
		"An empty object must not decode into a default constructed message");
}

SAW_TEST("Wire / a missing member is rejected at every nesting depth"){
	// Top level: seq absent.
	{
		auto eo = decode_message<sch::MsgPing>(
			R"({"type":"ping","worker_id":"w","sent_at_ms":1})");
		SAW_EXPECT(eo.is_error(), "A missing top level member must fail");
	}
	// One level down: live is present but incomplete. This is the case that
	// matters most, since strictness has to reach into nested structs or a
	// half filled capability would register cleanly.
	{
		auto eo = decode_message<sch::MsgPong>(
			R"({"type":"pong","worker_id":"w","seq":1,"sent_at_ms":1,)"
			R"("worker_now_ms":2,"live":{"state":"ready"}})");
		SAW_EXPECT(eo.is_error(), "A missing nested member must fail");
	}
	// And the complete form of the same message must still pass, so the test
	// above is not just rejecting everything.
	{
		auto eo = decode_message<sch::MsgPong>(
			R"({"type":"pong","worker_id":"w","seq":1,"sent_at_ms":1,)"
			R"("worker_now_ms":2,"live":{"state":"ready","uptime_ms":0,)"
			R"("steps_done":0,"last_error":""}})");
		SAW_EXPECT(not eo.is_error(), "The complete message must still decode");
	}
}

/**
 * Forward compatibility in the direction that works: a newer worker sending
 * members an older orchestrator has never heard of still registers.
 */
SAW_TEST("Wire / unknown members are skipped"){
	auto eo = decode_message<sch::MsgPing>(
		R"({"type":"ping","worker_id":"w","seq":3,"sent_at_ms":8,)"
		R"("future_field":{"nested":[1,2,3]},"another":"x"})");
	SAW_EXPECT(not eo.is_error(),
		"A newer sender's extra members must be skipped, not rejected");
	SAW_EXPECT(eo.get_value().get<"seq">().get() == 3u, "Known members still land");
	SAW_EXPECT(eo.get_value().get<"sent_at_ms">().get() == 8u, "and keep their values");
}

SAW_TEST("Wire / malformed input errors rather than defaulting"){
	const char* bad[] = {
		"",
		"{",
		R"({"type":"ping","worker_id":"w","seq":1,"sent_at_ms":1)",   // truncated
		R"({"type":"ping","worker_id":"w","seq":1,"sent_at_ms":1}})", // trailing
		R"({"type":"ping","worker_id":"w","seq":"not a number","sent_at_ms":1})",
		"[1,2,3]",
		"null",
	};

	for(const char* text : bad){
		auto eo = decode_message<sch::MsgPing>(text);
		SAW_EXPECT(eo.is_error(), std::string{"Malformed input must error: "} + text);
	}
}

/**
 * The capability envelope is the largest thing on the wire and the one whose
 * shape has to survive being read by a process built from different source.
 */
SAW_TEST("Wire / hello carries a capability round trip"){
	auto hello = make_message<sch::MsgHelloWebGl2>(msg::hello, "worker-9", 0u);

	auto& cap = hello.get<"capability">();
	cap.get<"worker_id">().set(std::string{"worker-9"});
	cap.get<"kind">().set(std::string{wkind::webgl2});
	cap.get<"schema_version">().set(std::string{schema_version});
	cap.get<"reported_at_ms">().set(1700000000000u);
	cap.get<"user_agent">().set(std::string{"test-agent"});

	auto& gl = cap.get<"webgl2">();

	auto& lim = gl.get<"limits">();
	lim.get<"max_texture_size">().set(16384u);
	lim.get<"max_3d_texture_size">().set(2048u);
	lim.get<"max_array_texture_layers">().set(2048u);
	lim.get<"max_draw_buffers">().set(8u);
	lim.get<"max_color_attachments">().set(8u);
	lim.get<"max_texture_image_units">().set(16u);
	lim.get<"max_combined_texture_image_units">().set(32u);
	lim.get<"max_renderbuffer_size">().set(16384u);
	lim.get<"max_fragment_uniform_vectors">().set(224u);
	lim.get<"max_viewport_dims">().at({{0u}}).set(16384u);
	lim.get<"max_viewport_dims">().at({{1u}}).set(16384u);

	auto& feat = gl.get<"features">();
	feat.get<"webgl2">().set(true);
	feat.get<"offscreen_canvas">().set(true);
	feat.get<"color_buffer_float">().set(true);
	feat.get<"color_buffer_half_float">().set(true);
	feat.get<"texture_float_linear">().set(false);
	feat.get<"float_blend">().set(false);
	feat.get<"parallel_shader_compile">().set(true);
	feat.get<"disjoint_timer_query">().set(false);

	auto& bud = gl.get<"budget">();
	bud.get<"probed_bytes_max">().set(536870912u);
	bud.get<"probed_bytes_safe">().set(322122547u);
	bud.get<"probe_trials">().set(14u);
	bud.get<"context_lost_during_probe">().set(false);
	bud.get<"probe_hit_texture_size_bound">().set(false);
	bud.get<"safety_factor">().set(0.6f);

	// One feasible plan, enough to prove the array of nested structs survives.
	{
		data<sch::PackingPlan> plan;
		plan.get<"layout">().set(std::string{"plain2d"});
		plan.get<"aux_mode">().set(std::string{"stuffed"});
		plan.get<"d">().set(2u);
		plan.get<"q">().set(9u);
		plan.get<"ghost">().set(0u);
		plan.get<"storage_bits">().set(32u);
		plan.get<"slots_per_texel">().set(4u);
		plan.get<"textures_df">().set(3u);
		plan.get<"textures_aux">().set(0u);
		plan.get<"states">().set(2u);
		plan.get<"passes">().set(1u);
		plan.get<"idle_slots">().set(3u);
		plan.get<"domain">().at({{0u}}).set(1024u);
		plan.get<"domain">().at({{1u}}).set(256u);
		plan.get<"domain">().at({{2u}}).set(1u);
		plan.get<"tiles">().at({{0u}}).set(1u);
		plan.get<"tiles">().at({{1u}}).set(1u);
		plan.get<"tiles_pow2">().set(true);
		plan.get<"texture_size">().at({{0u}}).set(1024u);
		plan.get<"texture_size">().at({{1u}}).set(256u);
		plan.get<"bytes_total">().set(1024u * 256u * 96u);
		plan.get<"bytes_useful">().set(1024u * 256u * 96u);

		for(uint64_t i = 0u; i < 9u; ++i){
			auto eo = plan.get<"slot_tex_of_q">().add(data<sch::UInt8>{static_cast<uint8_t>(i / 4u)});
			SAW_EXPECT(not eo.is_error(), "slot_tex_of_q grows");
			auto eo2 = plan.get<"slot_cmp_of_q">().add(data<sch::UInt8>{static_cast<uint8_t>(i % 4u)});
			SAW_EXPECT(not eo2.is_error(), "slot_cmp_of_q grows");
		}
		auto eo3 = plan.get<"slot_tex_of_aux">().add(data<sch::UInt8>{2u});
		SAW_EXPECT(not eo3.is_error(), "slot_tex_of_aux grows");
		auto eo4 = plan.get<"slot_cmp_of_aux">().add(data<sch::UInt8>{1u});
		SAW_EXPECT(not eo4.is_error(), "slot_cmp_of_aux grows");
		auto eo5 = plan.get<"aux_names">().add(data<sch::String>{std::string{"info"}});
		SAW_EXPECT(not eo5.is_error(), "aux_names grows");

		auto eo6 = gl.get<"feasible">().add(std::move(plan));
		SAW_EXPECT(not eo6.is_error(), "feasible grows");
	}

	auto eo_text = encode_message<sch::MsgHelloWebGl2>(hello);
	SAW_EXPECT(not eo_text.is_error(), "A complete capability must encode");

	// The header has to be readable out of the biggest message we have.
	auto eo_hdr = peek_header(eo_text.get_value());
	SAW_EXPECT(not eo_hdr.is_error(), "The header must survive a large payload");
	SAW_EXPECT(header_type(eo_hdr.get_value()) == msg::hello, "Header reports hello");

	// Routing pass: the common fields alone are enough to learn the kind, and
	// the webgl2 section is skipped because it is not named here.
	auto eo_route = decode_message<sch::MsgHello>(eo_text.get_value());
	SAW_EXPECT(not eo_route.is_error(), "The routing decode must succeed");
	SAW_EXPECT(eo_route.get_value().get<"capability">().get<"kind">().stl_view()
		== wkind::webgl2, "The routing decode must report the kind");

	auto eo_back = decode_message<sch::MsgHelloWebGl2>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "A complete capability must decode");
	auto& back = eo_back.get_value().get<"capability">();

	SAW_EXPECT(back.get<"kind">().stl_view() == wkind::webgl2, "kind survives");
	SAW_EXPECT(back.get<"webgl2">().get<"limits">().get<"max_texture_size">().get()
		== 16384u, "Deeply nested integers survive");
	SAW_EXPECT(back.get<"webgl2">().get<"features">().get<"color_buffer_float">().get(),
		"Booleans survive");
	SAW_EXPECT(back.get<"webgl2">().get<"budget">().get<"probe_trials">().get() == 14u,
		"Budget survives");

	auto& feasible = back.get<"webgl2">().get<"feasible">();
	SAW_EXPECT(feasible.meta().flat_at(0u).get() == 1u, "One plan came back");
	auto& plan_back = feasible.at({{0u}});
	SAW_EXPECT(plan_back.get<"q">().get() == 9u, "Plan Q survives");
	SAW_EXPECT(plan_back.get<"layout">().stl_view() == "plain2d", "Plan layout survives");
	SAW_EXPECT(plan_back.get<"slot_tex_of_q">().meta().flat_at(0u).get() == 9u,
		"The slot table keeps its length");
	SAW_EXPECT(plan_back.get<"slot_tex_of_q">().at({{8u}}).get() == 2u,
		"Direction 8 lives in texture 2");
	SAW_EXPECT(plan_back.get<"slot_cmp_of_q">().at({{8u}}).get() == 0u,
		"Direction 8 lives in channel 0");
	SAW_EXPECT(plan_back.get<"aux_names">().at({{0u}}).stl_view() == "info",
		"Arrays of strings survive");
}

/**
 * A worker of a kind we do not know about still has to be readable far enough
 * to find out that we do not know about it. This is the envelope earning its
 * keep.
 */
SAW_TEST("Wire / an unknown worker kind is still routable"){
	const char* text =
		R"({"type":"hello","worker_id":"w-x","seq":0,)"
		R"("capability":{"worker_id":"w-x","kind":"webgpu","schema_version":"1",)"
		R"("reported_at_ms":1,"user_agent":"a","webgpu":{"whatever":true}}})";

	auto eo_hdr = peek_header(text);
	SAW_EXPECT(not eo_hdr.is_error(),
		"An unknown backend section must not stop the header decoding");
	SAW_EXPECT(header_type(eo_hdr.get_value()) == msg::hello, "Still a hello");
	SAW_EXPECT(header_worker_id(eo_hdr.get_value()) == "w-x", "Still identifiable");

	// The routing decode succeeds and names the kind, which is exactly what it
	// is for: an unusable worker is still an identifiable worker.
	auto eo_route = decode_message<sch::MsgHello>(text);
	SAW_EXPECT(not eo_route.is_error(),
		"The common fields of an unknown kind must still decode");
	SAW_EXPECT(eo_route.get_value().get<"capability">().get<"kind">().stl_view()
		== "webgpu", "The orchestrator learns which kind it cannot use");

	// Decoding it as a webgl2 hello must fail, because "webgl2" is absent --
	// that is the orchestrator learning it cannot use this worker, which is a
	// different and better outcome than a zeroed capability.
	auto eo_full = decode_message<sch::MsgHelloWebGl2>(text);
	SAW_EXPECT(eo_full.is_error(),
		"A webgl2 decode of a webgpu worker must fail rather than zero fill");
}
/**
 * Native workers are a worker kind, not a test fixture, so the same routing has
 * to work for them -- and crucially a native hello must not be obliged to carry
 * a texture budget it has no way to measure.
 */
SAW_TEST("Wire / a native worker registers without a webgl section"){
	auto hello = make_message<sch::MsgHelloNative>(msg::hello, "n-1", 0u);

	auto& cap = hello.get<"capability">();
	cap.get<"worker_id">().set(std::string{"n-1"});
	cap.get<"kind">().set(std::string{wkind::native});
	cap.get<"schema_version">().set(std::string{schema_version});
	cap.get<"reported_at_ms">().set(1700000000000u);
	cap.get<"user_agent">().set(std::string{"kel-lbm-worker-native"});

	auto& nat = cap.get<"native">();
	nat.get<"threads">().set(16u);
	nat.get<"memory_bytes">().set(34359738368u);
	nat.get<"cpu">().set(std::string{"x86_64"});
	nat.get<"has_sycl">().set(false);

	auto eo_text = encode_message<sch::MsgHelloNative>(hello);
	SAW_EXPECT(not eo_text.is_error(), "A native hello must encode");

	// Routes the same way a browser does.
	auto eo_route = decode_message<sch::MsgHello>(eo_text.get_value());
	SAW_EXPECT(not eo_route.is_error(), "A native hello must route");
	SAW_EXPECT(eo_route.get_value().get<"capability">().get<"kind">().stl_view()
		== wkind::native, "Routing reports native");

	auto eo_back = decode_message<sch::MsgHelloNative>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "A native hello must decode");
	SAW_EXPECT(eo_back.get_value().get<"capability">().get<"native">()
		.get<"threads">().get() == 16u, "Thread count survives");

	// And the point of the restructure: this text has no webgl2 section at all,
	// so a decode that demands one has to fail rather than zero fill.
	auto eo_wrong = decode_message<sch::MsgHelloWebGl2>(eo_text.get_value());
	SAW_EXPECT(eo_wrong.is_error(),
		"A native worker must not decode as a webgl2 one");
}
}
