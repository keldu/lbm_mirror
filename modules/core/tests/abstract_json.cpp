#include <forstio/test/suite.hpp>

#include "../c++/abstract/json.hpp"

namespace {
namespace ts {
using namespace kel::lbm::sch;
}

using kel::lbm::codec;
using kel::lbm::data;
using kel::lbm::enc::Json;
using kel::lbm::enc::Native;

template<typename Schema>
std::string encode_to_string(const data<Schema>& value, bool pretty = false){
	data<Schema, Json> out;
	codec<Schema, Json> c;
	c.set_pretty(pretty);
	auto eov = c.encode(value, out);
	SAW_EXPECT(eov.is_value(), "encoding succeeded");
	return out.stl_string();
}

SAW_TEST("Abstract/Json/EncodePrimitives"){
	SAW_EXPECT(encode_to_string(data<ts::UInt64>{42u}) == "42", "unsigned");
	SAW_EXPECT(encode_to_string(data<ts::Int32>{-7}) == "-7", "signed");
	SAW_EXPECT(encode_to_string(data<ts::Bool>{true}) == "true", "bool true");
	SAW_EXPECT(encode_to_string(data<ts::Bool>{false}) == "false", "bool false");
}

SAW_TEST("Abstract/Json/EncodeString"){
	data<ts::String> s{std::string{"a\"b\\c\nd"}};
	// Quote, backslash and newline all have to come back escaped.
	SAW_EXPECT(encode_to_string(s) == "\"a\\\"b\\\\c\\nd\"", "escapes applied");
}

SAW_TEST("Abstract/Json/DecodePrimitives"){
	data<ts::UInt64> u;
	codec<ts::UInt64, Json> cu;
	SAW_EXPECT(cu.decode(data<ts::UInt64,Json>{std::string{"123"}}, u).is_value(), "decoded");
	SAW_EXPECT(u.get() == 123u, "unsigned round trips");

	data<ts::Float64> f;
	codec<ts::Float64, Json> cf;
	SAW_EXPECT(cf.decode(data<ts::Float64,Json>{std::string{"-0.5e1"}}, f).is_value(), "decoded");
	SAW_EXPECT(f.get() == -5.0, "exponent form is understood");
}

SAW_TEST("Abstract/Json/FloatRoundTrip"){
	// max_digits10 precision is what makes this exact rather than approximate.
	const double original = 0.1234567890123456789;
	data<ts::Float64> in{original};

	data<ts::Float64> out;
	codec<ts::Float64, Json> c;
	auto eov = c.decode(data<ts::Float64,Json>{encode_to_string(in)}, out);
	SAW_EXPECT(eov.is_value(), "round trip decoded");
	SAW_EXPECT(out.get() == in.get(), "a double survives a save and load cycle");
}

SAW_TEST("Abstract/Json/IntegerRejectsFraction"){
	// Truncating 1.5 into an integer field would lose data quietly.
	data<ts::UInt64> u;
	codec<ts::UInt64, Json> c;
	auto eov = c.decode(data<ts::UInt64,Json>{std::string{"1.5"}}, u);
	SAW_EXPECT(eov.is_error(), "a fractional value is not an integer");
}

SAW_TEST("Abstract/Json/UnsignedRejectsNegative"){
	data<ts::UInt64> u;
	codec<ts::UInt64, Json> c;
	auto eov = c.decode(data<ts::UInt64,Json>{std::string{"-1"}}, u);
	SAW_EXPECT(eov.is_error(), "a negative value is not unsigned");
}

SAW_TEST("Abstract/Json/EncodeAndDecodeStruct"){
	using TStruct = ts::Struct<
		ts::Member<ts::UInt64,"count">,
		ts::Member<ts::Float64,"weight">
	>;

	data<TStruct> in;
	in.get<"count">().set(3u);
	in.get<"weight">().set(0.5);

	auto text = encode_to_string(in);
	SAW_EXPECT(text == "{\"count\":3,\"weight\":0.5}", "object layout");

	data<TStruct> out;
	codec<TStruct, Json> c;
	auto eov = c.decode(data<TStruct,Json>{text}, out);
	SAW_EXPECT(eov.is_value(), "decoded");
	SAW_EXPECT(out.get<"count">().get() == 3u, "count round trips");
	SAW_EXPECT(out.get<"weight">().get() == 0.5, "weight round trips");
}

SAW_TEST("Abstract/Json/StructIgnoresUnknownKeys"){
	using TStruct = ts::Struct<ts::Member<ts::UInt64,"count">>;

	data<TStruct> out;
	codec<TStruct, Json> c;
	// A nested unknown value has to be skipped whole, not just one token.
	auto eov = c.decode(
		data<TStruct,Json>{std::string{
			"{\"extra\":{\"a\":[1,2,{\"b\":\"x\"}]},\"count\":9}"}},
		out);

	SAW_EXPECT(eov.is_value(), "unknown members are skipped");
	SAW_EXPECT(out.get<"count">().get() == 9u, "known member still lands");
}

SAW_TEST("Abstract/Json/StructMissingMember"){
	using TStruct = ts::Struct<
		ts::Member<ts::UInt64,"count">,
		ts::Member<ts::UInt64,"other">
	>;

	// Lenient by default: a config states only its deviations.
	data<TStruct> lenient;
	codec<TStruct, Json> c;
	auto eov = c.decode(data<TStruct,Json>{std::string{"{\"count\":1}"}}, lenient);
	SAW_EXPECT(eov.is_value(), "a missing member is allowed by default");
	SAW_EXPECT(lenient.get<"other">().get() == 0u, "and keeps its default");

	data<TStruct> strict;
	codec<TStruct, Json> cs;
	cs.set_strict(true);
	auto eov_s = cs.decode(data<TStruct,Json>{std::string{"{\"count\":1}"}}, strict);
	SAW_EXPECT(eov_s.is_error(), "strict decoding demands every member");
}

SAW_TEST("Abstract/Json/FixedArray"){
	using TArr = ts::FixedArray<ts::UInt64,3u>;

	data<TArr> in{1u,2u,3u};
	auto text = encode_to_string(in);
	SAW_EXPECT(text == "[1,2,3]", "array layout");

	data<TArr> out;
	codec<TArr, Json> c;
	SAW_EXPECT(c.decode(data<TArr,Json>{text}, out).is_value(), "decoded");
	SAW_EXPECT(out.flat_at(2u).get() == 3u, "elements round trip");
}

SAW_TEST("Abstract/Json/FixedArrayLengthMismatch"){
	using TArr = ts::FixedArray<ts::UInt64,3u>;
	data<TArr> out;

	codec<TArr, Json> c_short;
	SAW_EXPECT(c_short.decode(data<TArr,Json>{std::string{"[1,2]"}}, out).is_error(),
		"too few elements is an error, there is no default to fill with");

	codec<TArr, Json> c_long;
	SAW_EXPECT(c_long.decode(data<TArr,Json>{std::string{"[1,2,3,4]"}}, out).is_error(),
		"too many elements is an error, there is nowhere to put them");
}

SAW_TEST("Abstract/Json/DynamicArrayGrows"){
	using TArr = ts::Array<ts::UInt64,1u>;

	data<TArr> out;
	codec<TArr, Json> c;
	SAW_EXPECT(c.decode(data<TArr,Json>{std::string{"[5,6,7,8]"}}, out).is_value(), "decoded");
	SAW_EXPECT(out.size().get() == 4u, "array grew to fit the document");
	SAW_EXPECT(out.at({3u}).get() == 8u, "last element landed");
}

SAW_TEST("Abstract/Json/Tuple"){
	using TTup = ts::Tuple<ts::UInt64, ts::Bool>;

	data<TTup> in;
	in.get<0u>().set(4u);
	in.get<1u>().set(true);

	auto text = encode_to_string(in);
	SAW_EXPECT(text == "[4,true]", "tuple is an array");

	data<TTup> out;
	codec<TTup, Json> c;
	SAW_EXPECT(c.decode(data<TTup,Json>{text}, out).is_value(), "decoded");
	SAW_EXPECT(out.get<0u>().get() == 4u, "first element");
	SAW_EXPECT(out.get<1u>().get() == true, "second element");
}

SAW_TEST("Abstract/Json/Tensor"){
	using TVec = ts::Vector<ts::Float64,3u>;

	data<TVec> in{1.0, 2.0, 3.0};
	auto text = encode_to_string(in);

	data<TVec> out;
	codec<TVec, Json> c;
	SAW_EXPECT(c.decode(data<TVec,Json>{text}, out).is_value(), "decoded");
	SAW_EXPECT(out.flat_at(1u).get() == 2.0, "components round trip");
}

SAW_TEST("Abstract/Json/Nested"){
	using TInner = ts::Struct<ts::Member<ts::UInt64,"n">>;
	using TOuter = ts::Struct<
		ts::Member<TInner,"inner">,
		ts::Member<ts::FixedArray<ts::UInt64,2u>,"pair">
	>;

	data<TOuter> in;
	in.get<"inner">().get<"n">().set(1u);
	in.get<"pair">().flat_at(0u).set(2u);
	in.get<"pair">().flat_at(1u).set(3u);

	auto text = encode_to_string(in);
	SAW_EXPECT(text == "{\"inner\":{\"n\":1},\"pair\":[2,3]}", "nested layout");

	data<TOuter> out;
	codec<TOuter, Json> c;
	SAW_EXPECT(c.decode(data<TOuter,Json>{text}, out).is_value(), "decoded");
	SAW_EXPECT(out.get<"inner">().get<"n">().get() == 1u, "nested member");
	SAW_EXPECT(out.get<"pair">().flat_at(1u).get() == 3u, "nested array");
}

SAW_TEST("Abstract/Json/Whitespace"){
	using TStruct = ts::Struct<ts::Member<ts::UInt64,"count">>;

	data<TStruct> out;
	codec<TStruct, Json> c;
	auto eov = c.decode(
		data<TStruct,Json>{std::string{"  {\n\t\"count\" : 5\r\n}  "}}, out);

	SAW_EXPECT(eov.is_value(), "whitespace around tokens is ignored");
	SAW_EXPECT(out.get<"count">().get() == 5u, "value still read");
}

SAW_TEST("Abstract/Json/RejectsTrailingContent"){
	data<ts::UInt64> out;
	codec<ts::UInt64, Json> c;
	auto eov = c.decode(data<ts::UInt64,Json>{std::string{"1 2"}}, out);
	SAW_EXPECT(eov.is_error(), "content after the value means a malformed document");
}

SAW_TEST("Abstract/Json/RejectsMalformed"){
	using TStruct = ts::Struct<ts::Member<ts::UInt64,"count">>;
	data<TStruct> out;

	codec<TStruct, Json> c_unclosed;
	SAW_EXPECT(c_unclosed.decode(data<TStruct,Json>{std::string{"{\"count\":1"}}, out).is_error(),
		"unterminated object");

	codec<TStruct, Json> c_empty;
	SAW_EXPECT(c_empty.decode(data<TStruct,Json>{std::string{""}}, out).is_error(),
		"empty input");

	codec<TStruct, Json> c_garbage;
	SAW_EXPECT(c_garbage.decode(data<TStruct,Json>{std::string{"{\"count\":}"}}, out).is_error(),
		"missing value");
}

SAW_TEST("Abstract/Json/DepthLimit"){
	using TArr = ts::Array<ts::UInt64,1u>;

	// The guard is against pathological nesting in untrusted input; a flat
	// document sits well inside it.
	data<TArr> out;
	kel::lbm::codec_config<Json> cfg;
	cfg.depth = 1u;
	codec<TArr, Json> c{cfg};
	SAW_EXPECT(c.decode(data<TArr,Json>{std::string{"[1,2]"}}, out).is_value(),
		"one level of nesting is within a depth of one");
}

SAW_TEST("Abstract/Json/StringEscapeRoundTrip"){
	data<ts::String> in{std::string{"tab\there \"quoted\" back\\slash"}};

	data<ts::String> out;
	codec<ts::String, Json> c;
	auto eov = c.decode(data<ts::String,Json>{encode_to_string(in)}, out);

	SAW_EXPECT(eov.is_value(), "decoded");
	SAW_EXPECT(out.stl_view() == in.stl_view(), "escapes survive a round trip");
}

SAW_TEST("Abstract/Json/UnicodeEscape"){
	data<ts::String> out;
	codec<ts::String, Json> c;
	// U+00E9 encodes to two UTF-8 bytes.
	auto eov = c.decode(data<ts::String,Json>{std::string{"\"\\u00e9\""}}, out);

	SAW_EXPECT(eov.is_value(), "decoded");
	SAW_EXPECT(out.size().get() == 2u, "decoded to UTF-8 rather than one byte");
}

SAW_TEST("Abstract/Json/PrettyPrint"){
	using TStruct = ts::Struct<ts::Member<ts::UInt64,"count">>;

	data<TStruct> in;
	in.get<"count">().set(1u);

	auto text = encode_to_string(in, true);
	SAW_EXPECT(text == "{\n  \"count\": 1\n}", "pretty layout");

	// Pretty output still has to parse back.
	data<TStruct> out;
	codec<TStruct, Json> c;
	SAW_EXPECT(c.decode(data<TStruct,Json>{text}, out).is_value(), "reparsed");
	SAW_EXPECT(out.get<"count">().get() == 1u, "value preserved");
}

SAW_TEST("Abstract/Json/LbmConfigShape"){
	// The shape config.hpp actually loads.
	using TConfig = ts::Struct<
		ts::Member<ts::Float64, "delta_x">,
		ts::Member<ts::Float64, "kinematic_viscosity">,
		ts::Member<ts::Float64, "delta_t">
	>;

	data<TConfig> out;
	codec<TConfig, Json> c;
	auto eov = c.decode(
		data<TConfig,Json>{std::string{
			"{\"delta_x\":0.01,\"kinematic_viscosity\":1.5e-5,\"delta_t\":0.001}"}},
		out);

	SAW_EXPECT(eov.is_value(), "a config document decodes");
	SAW_EXPECT(out.get<"delta_x">().get() == 0.01, "delta_x");
	SAW_EXPECT(out.get<"kinematic_viscosity">().get() == 1.5e-5, "viscosity");
	SAW_EXPECT(out.get<"delta_t">().get() == 0.001, "delta_t");
}
}
