#include <forstio/test/suite.hpp>

#include "../c++/signal.hpp"
#include "../c++/wire.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;
using namespace kel::lbm::swarm;

SAW_TEST("Signal / an offer round trips"){
	data<sch::SignalDescription> desc;
	desc.get<"type">().set(std::string{sig::offer});
	desc.get<"sdp">().set(std::string{"v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\n"});

	auto eo_text = encode_message<sch::SignalDescription>(desc);
	SAW_EXPECT(not eo_text.is_error(), "An offer must encode");

	auto eo_back = decode_message<sch::SignalDescription>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "An offer must decode");
	SAW_EXPECT(eo_back.get_value().get<"type">().stl_view() == sig::offer,
		"type survives");
	// SDP is multi-line and full of characters JSON has to escape; that it
	// comes back byte for byte is the only interesting property here.
	SAW_EXPECT(eo_back.get_value().get<"sdp">().stl_view()
		== "v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\n",
		"SDP must survive escaping unchanged");
}

SAW_TEST("Signal / a candidate round trips"){
	data<sch::SignalCandidate> cand;
	cand.get<"type">().set(std::string{sig::candidate});
	cand.get<"candidate">().set(std::string{"candidate:1 1 UDP 100 10.0.0.1 5000 typ host"});
	cand.get<"mid">().set(std::string{"0"});

	auto eo_text = encode_message<sch::SignalCandidate>(cand);
	SAW_EXPECT(not eo_text.is_error(), "A candidate must encode");

	auto eo_back = decode_message<sch::SignalCandidate>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "A candidate must decode");
	SAW_EXPECT(eo_back.get_value().get<"mid">().stl_view() == "0", "mid survives");
}

SAW_TEST("Signal / the header routes without knowing the payload"){
	data<sch::SignalDescription> desc;
	desc.get<"type">().set(std::string{sig::answer});
	desc.get<"sdp">().set(std::string{"v=0"});
	auto eo_text = encode_message<sch::SignalDescription>(desc);
	SAW_EXPECT(not eo_text.is_error(), "encodes");

	auto eo_hdr = decode_message<sch::SignalHeader>(eo_text.get_value());
	SAW_EXPECT(not eo_hdr.is_error(), "A header read must skip the sdp");
	SAW_EXPECT(eo_hdr.get_value().get<"type">().stl_view() == sig::answer,
		"Header reports answer");
}

SAW_TEST("Signal / an empty mid is carried rather than dropped"){
	// libdatachannel accepts an unhinted candidate, so the empty string has to
	// survive the round trip as an empty string and not become a decode error.
	data<sch::SignalCandidate> cand;
	cand.get<"type">().set(std::string{sig::candidate});
	cand.get<"candidate">().set(std::string{"candidate:1 1 UDP 100 10.0.0.1 5000 typ host"});
	cand.get<"mid">().set(std::string{""});

	auto eo_text = encode_message<sch::SignalCandidate>(cand);
	SAW_EXPECT(not eo_text.is_error(), "encodes");
	auto eo_back = decode_message<sch::SignalCandidate>(eo_text.get_value());
	SAW_EXPECT(not eo_back.is_error(), "An empty mid must still decode");
	SAW_EXPECT(eo_back.get_value().get<"mid">().stl_view().empty(),
		"An empty mid stays empty");
}

SAW_TEST("Signal / a candidate missing its mid is rejected"){
	// Strictness applies here too: a signalling message that lost a member in
	// transit should fail loudly rather than add a candidate with no mid.
	auto eo = decode_message<sch::SignalCandidate>(
		R"({"type":"candidate","candidate":"candidate:1 1 UDP 100 10.0.0.1 5000 typ host"})");
	SAW_EXPECT(eo.is_error(), "A missing mid must be an error");
}
}
