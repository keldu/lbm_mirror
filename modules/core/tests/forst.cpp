#include <forstio/test/suite.hpp>

#include "../c++/abstract/forst.hpp"
#include "../c++/forst_chunk.hpp"

#include <cstring>
#include <string>

namespace {
namespace sch {
using namespace kel::lbm::sch;
}
using namespace kel::lbm;

template <typename Schema>
error_or<void> round_trip(const data<Schema> &from, data<Schema> &to){
	codec<Schema, enc::Forst> c;

	data<Schema, enc::Forst> bytes;
	auto eov = c.encode(from, bytes);
	if(eov.is_error()){
		return eov;
	}
	return c.decode(bytes, to);
}

template <typename Schema>
uint64_t encoded_size(const data<Schema> &from){
	codec<Schema, enc::Forst> c;
	data<Schema, enc::Forst> bytes;
	auto eov = c.encode(from, bytes);
	(void) eov;
	return bytes.size().get();
}

SAW_TEST("Forst / layout traits match the declared table"){
	using namespace kel::lbm::impl;

	static_assert(forst_info<sch::UInt8>::static_size == 1u, "u8");
	static_assert(forst_info<sch::Float32>::static_size == 4u, "f32");
	static_assert(forst_info<sch::Float64>::static_size == 8u, "f64");
	static_assert(forst_info<sch::UInt8>::layers == 0u, "primitives are fixed");

	static_assert(forst_info<sch::String>::static_size == 16u, "length + offset");
	static_assert(forst_info<sch::String>::layers == 1u, "a string is dynamic");

	static_assert(forst_info<sch::Array<sch::UInt64, 1u>>::static_size == 16u,
		"one extent + offset");
	static_assert(forst_info<sch::Array<sch::UInt64, 2u>>::static_size == 24u,
		"two extents + offset");

	static_assert(forst_info<sch::FixedArray<sch::Float32, 9u>>::static_size == 36u,
		"a fixed array is its elements inline");
	static_assert(forst_info<sch::FixedArray<sch::Float32, 9u>>::layers == 0u,
		"and stays fixed");

	SAW_EXPECT(true, "static asserts carry this one");
}

/**
 * The property the whole design rests on: an array of a dynamic element needs
 * a second layer, and must therefore be rejected rather than mis-encoded.
 */
SAW_TEST("Forst / nesting depth is counted, so two layers are detectable"){
	using namespace kel::lbm::impl;

	static_assert(forst_info<sch::Array<sch::String, 1u>>::layers == 2u,
		"Array<String> is two layers and must not pass the codec's assert");
	static_assert(
		forst_info<sch::Array<sch::Array<sch::UInt64,1u>, 1u>>::layers == 2u,
		"Array<Array<...>> likewise");

	using Flat = sch::Struct<
		sch::Member<sch::String, "a">,
		sch::Member<sch::Array<sch::UInt64, 1u>, "b">
	>;
	static_assert(forst_info<Flat>::layers == 1u,
		"Several dynamic members side by side are still one layer");

	// And the thing the result manifest depends on: an array of a *fixed*
	// struct stays at one layer.
	using FixedElem = sch::Struct<
		sch::Member<sch::FixedArray<sch::UInt8, 32u>, "name">,
		sch::Member<sch::UInt64, "bytes">
	>;
	static_assert(forst_info<FixedElem>::layers == 0u, "fixed struct");
	static_assert(forst_info<sch::Array<FixedElem, 1u>>::layers == 1u,
		"an array of fixed structs is what keeps the manifest flat");

	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Forst / primitives round trip exactly"){
	{
		data<sch::Float32> from; from.set(-1.5f);
		data<sch::Float32> to;
		SAW_EXPECT_EOV(round_trip(from, to), "f32");
		SAW_EXPECT(to.get() == -1.5f, "f32 survives bit for bit");
	}
	{
		data<sch::Float64> from; from.set(1.0 / 3.0);
		data<sch::Float64> to;
		SAW_EXPECT_EOV(round_trip(from, to), "f64");
		SAW_EXPECT(to.get() == 1.0 / 3.0, "f64 survives bit for bit");
	}
	{
		// The edges, where a width or sign mistake would show.
		data<sch::UInt64> from; from.set(~uint64_t{0u});
		data<sch::UInt64> to;
		SAW_EXPECT_EOV(round_trip(from, to), "u64");
		SAW_EXPECT(to.get() == ~uint64_t{0u}, "u64 max survives");
	}
	{
		data<sch::Int32> from; from.set(-2147483647 - 1);
		data<sch::Int32> to;
		SAW_EXPECT_EOV(round_trip(from, to), "i32");
		SAW_EXPECT(to.get() == -2147483647 - 1, "i32 min survives");
	}
	{
		data<sch::Bool> from; from.set(true);
		data<sch::Bool> to;
		SAW_EXPECT_EOV(round_trip(from, to), "bool");
		SAW_EXPECT(to.get(), "bool survives");
		SAW_EXPECT(encoded_size(from) == 1u, "and costs one byte");
	}
}

SAW_TEST("Forst / strings round trip, including the empty one"){
	{
		data<sch::String> from{std::string{"velocity"}};
		data<sch::String> to;
		SAW_EXPECT_EOV(round_trip(from, to), "string");
		SAW_EXPECT(to.stl_view() == "velocity", "contents survive");
		SAW_EXPECT(encoded_size(from) == 16u + 8u, "slot plus contents");
	}
	{
		data<sch::String> from{std::string{""}};
		data<sch::String> to{std::string{"not empty"}};
		SAW_EXPECT_EOV(round_trip(from, to), "empty string");
		SAW_EXPECT(to.stl_view().empty(), "an empty string stays empty");
		SAW_EXPECT(encoded_size(from) == 16u, "and costs only its slot");
	}
	{
		// Embedded NULs and high bytes: this is a byte string, not C text.
		std::string raw{"a\0b\xff", 4u};
		data<sch::String> from{raw};
		data<sch::String> to;
		SAW_EXPECT_EOV(round_trip(from, to), "binary string");
		SAW_EXPECT(to.stl_string() == raw, "NULs and high bytes survive");
	}
}

SAW_TEST("Forst / several dynamic members share the one layer"){
	using Schema = sch::Struct<
		sch::Member<sch::String, "first">,
		sch::Member<sch::UInt32, "middle">,
		sch::Member<sch::String, "last">
	>;

	data<Schema> from;
	from.get<"first">().set(std::string{"alpha"});
	from.get<"middle">().set(42u);
	from.get<"last">().set(std::string{"omega"});

	data<Schema> to;
	SAW_EXPECT_EOV(round_trip(from, to), "struct with two strings");

	SAW_EXPECT(to.get<"first">().stl_view() == "alpha", "first survives");
	SAW_EXPECT(to.get<"middle">().get() == 42u, "the fixed member between them survives");
	SAW_EXPECT(to.get<"last">().stl_view() == "omega", "and so does the second string");

	// 16 + 4 + 16 static, then 5 + 5 of contents.
	SAW_EXPECT(encoded_size(from) == 36u + 10u, "no padding anywhere");
}

SAW_TEST("Forst / an array of fixed structs round trips"){
	using Elem = sch::Struct<
		sch::Member<sch::UInt32, "id">,
		sch::Member<sch::Float32, "value">
	>;
	using Schema = sch::Struct<sch::Member<sch::Array<Elem, 1u>, "items">>;

	data<Schema> from;
	for(uint32_t i = 0u; i < 4u; ++i){
		data<Elem> elem;
		elem.get<"id">().set(i);
		elem.get<"value">().set(static_cast<float>(i) * 0.5f);
		auto eov = from.get<"items">().add(std::move(elem));
		SAW_EXPECT_EOV(eov, "array grows");
	}

	data<Schema> to;
	SAW_EXPECT_EOV(round_trip(from, to), "array of structs");

	SAW_EXPECT(to.get<"items">().meta().flat_at(0u).get() == 4u,
		"the extent survives");
	for(uint64_t i = 0u; i < 4u; ++i){
		SAW_EXPECT(to.get<"items">().at({{i}}).get<"id">().get() == i,
			"element ids survive");
		SAW_EXPECT(to.get<"items">().at({{i}}).get<"value">().get()
			== static_cast<float>(i) * 0.5f, "element values survive");
	}

	// 16 static for the slot, 8 per element.
	SAW_EXPECT(encoded_size(from) == 16u + 4u * 8u, "exact, unpadded");
}

/**
 * The claim the whole encoding choice rests on: a struct of chunks is entirely
 * fixed size, so its Forst form is the packed field data itself. If this ever
 * stops holding, sending results costs a serialisation pass and a second
 * buffer -- in a browser tab, possibly one that does not exist.
 */
SAW_TEST("Forst / a struct of chunks is exactly its raw bytes"){
	constexpr uint64_t dim_x = 16u;
	constexpr uint64_t dim_y = 8u;

	using Velocity = sch::Chunk<sch::Vector<sch::Float32, 2u>, 0u, dim_x, dim_y>;
	using Density  = sch::Chunk<sch::Scalar<sch::Float32>, 0u, dim_x, dim_y>;
	using Macro = sch::Struct<
		sch::Member<Velocity, "velocity">,
		sch::Member<Density, "density">
	>;

	// The standard-layout assumption the zero-copy path depends on.
	static_assert(sizeof(data<sch::Vector<sch::Float32, 2u>>) == 2u * sizeof(float),
		"A Vector must be its components with nothing around them");
	static_assert(sizeof(data<sch::Scalar<sch::Float32>>) == sizeof(float),
		"A Scalar must be its component with nothing around it");

	static_assert(kel::lbm::impl::forst_info<Macro>::layers == 0u,
		"A struct of chunks has no dynamic part at all");

	constexpr uint64_t expected = dim_x * dim_y * (2u + 1u) * 4u;
	static_assert(kel::lbm::impl::forst_info<Macro>::static_size == expected,
		"and occupies exactly the field data");

	data<Macro> from;
	for(uint64_t y = 0u; y < dim_y; ++y){
		for(uint64_t x = 0u; x < dim_x; ++x){
			auto &vel = from.get<"velocity">().at({{x, y}});
			vel.at({{0u}}).set(static_cast<float>(x));
			vel.at({{1u}}).set(static_cast<float>(y));
			from.get<"density">().at({{x, y}}).at({}).set(
				static_cast<float>(x * dim_y + y));
		}
	}

	codec<Macro, enc::Forst> c;
	data<Macro, enc::Forst> bytes;
	SAW_EXPECT_EOV(c.encode(from, bytes), "encode");

	SAW_EXPECT(bytes.size().get() == expected,
		"The encoding adds not one byte of framing");

	// And the bytes are the buffer, not merely the same size as it.
	const auto *velocity_raw = reinterpret_cast<const uint8_t *>(
		from.get<"velocity">().flat_data());
	SAW_EXPECT(std::memcmp(bytes.raw(), velocity_raw, dim_x * dim_y * 2u * 4u) == 0,
		"The velocity chunk encodes to its own memory, byte for byte");

	const auto *density_raw = reinterpret_cast<const uint8_t *>(
		from.get<"density">().flat_data());
	SAW_EXPECT(std::memcmp(bytes.raw() + dim_x * dim_y * 2u * 4u, density_raw,
						   dim_x * dim_y * 4u) == 0,
		"and the density chunk follows it with no gap");

	data<Macro> to;
	SAW_EXPECT_EOV(c.decode(bytes, to), "decode");
	SAW_EXPECT(to.get<"velocity">().at({{3u, 5u}}).at({{1u}}).get() == 5.0f,
		"values come back where they went in");
	SAW_EXPECT(to.get<"density">().at({{3u, 5u}}).at({}).get()
		== static_cast<float>(3u * dim_y + 5u), "for every member");
}

SAW_TEST("Forst / a chunk's ghost ring is part of the encoding"){
	// Ghost=1 widens each side by two, and flat_data covers the ring. A reader
	// that assumed the logical extents would be short by the whole border.
	using Df = sch::Chunk<sch::FixedArray<sch::Float32, 9u>, 1u, 8u, 4u>;

	constexpr uint64_t stored = (8u + 2u) * (4u + 2u);
	static_assert(kel::lbm::impl::forst_info<Df>::static_size == stored * 9u * 4u,
		"The encoded size is the padded extents, not the logical ones");

	// Wrapped in a Struct rather than encoded bare: chunk.hpp already
	// specialises data<Chunk<...>, Encode> for any encoding, so a top level
	// data<Chunk, enc::Forst> is ambiguous against it. enc::Json has the same
	// clash. A field is always a member of a struct in practice.
	using Field = sch::Struct<sch::Member<Df, "dfs">>;

	data<Field> from;
	from.get<"dfs">().at({{0u, 0u}}).at({3u}).set(7.0f);

	data<Field> to;
	SAW_EXPECT_EOV(round_trip(from, to), "df chunk");
	SAW_EXPECT(to.get<"dfs">().at({{0u, 0u}}).at({3u}).get() == 7.0f,
		"interior survives");
}

SAW_TEST("Forst / truncated input is rejected rather than read past"){
	using Schema = sch::Struct<
		sch::Member<sch::String, "name">,
		sch::Member<sch::UInt64, "count">
	>;

	data<Schema> from;
	from.get<"name">().set(std::string{"something"});
	from.get<"count">().set(5u);

	codec<Schema, enc::Forst> c;
	data<Schema, enc::Forst> bytes;
	SAW_EXPECT_EOV(c.encode(from, bytes), "encode");

	// Cut the contents off and the string's length no longer fits the buffer.
	auto truncated = bytes.bytes();
	truncated.resize(truncated.size() - 4u);
	data<Schema, enc::Forst> damaged{std::move(truncated)};

	data<Schema> to;
	SAW_EXPECT(c.decode(damaged, to).is_error(),
		"A string reaching past the end must be an error, not a read");

	// Cut below the static section and it must fail before indexing anything.
	data<Schema, enc::Forst> stub{std::vector<uint8_t>(4u, 0u)};
	SAW_EXPECT(c.decode(stub, to).is_error(),
		"A buffer shorter than the static section must be rejected up front");
	SAW_EXPECT(c.decode(data<Schema, enc::Forst>{}, to).is_error(),
		"and so must an empty one");
}

SAW_TEST("Forst / a hostile offset cannot escape the buffer"){
	using Schema = sch::Struct<sch::Member<sch::String, "name">>;

	data<Schema> from;
	from.get<"name">().set(std::string{"abc"});

	codec<Schema, enc::Forst> c;
	data<Schema, enc::Forst> bytes;
	SAW_EXPECT_EOV(c.encode(from, bytes), "encode");

	// A length near the top of the range must not wrap when added to a start
	// that is itself inside the buffer.
	{
		auto tampered = bytes.bytes();
		const uint64_t huge = ~uint64_t{0u} - 8u;
		std::memcpy(tampered.data(), &huge, 8u);
		data<Schema, enc::Forst> evil{std::move(tampered)};
		data<Schema> to;
		SAW_EXPECT(c.decode(evil, to).is_error(), "An absurd length is rejected");
	}
	// An offset pointing far outside likewise.
	{
		auto tampered = bytes.bytes();
		const uint64_t far = ~uint64_t{0u} / 2u;
		std::memcpy(tampered.data() + 8u, &far, 8u);
		data<Schema, enc::Forst> evil{std::move(tampered)};
		data<Schema> to;
		SAW_EXPECT(c.decode(evil, to).is_error(), "An absurd offset is rejected");
	}
}
}
