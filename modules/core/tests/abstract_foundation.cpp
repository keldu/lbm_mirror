#include <forstio/test/suite.hpp>

#include "../c++/abstract/common.hpp"
#include "../c++/abstract/error.hpp"
#include "../c++/abstract/schema.hpp"
#include "../c++/abstract/stringify_schema.hpp"
#include "../c++/abstract/templates.hpp"

namespace {
namespace ts {
using namespace kel::lbm::sch;
}

SAW_TEST("Abstract/Templates/CtMultiply"){
	using namespace kel::lbm;

	static_assert(ct_multiply<uint64_t>::value == 1u, "empty pack is the identity");
	static_assert(ct_multiply<uint64_t,3u>::value == 3u, "");
	static_assert(ct_multiply<uint64_t,2u,3u,4u>::value == 24u, "");
}

SAW_TEST("Abstract/Templates/ParameterPack"){
	using namespace kel::lbm;

	static_assert(std::is_same_v<parameter_pack_type<0u, int, char, bool>::type, int>, "");
	static_assert(std::is_same_v<parameter_pack_type<2u, int, char, bool>::type, bool>, "");

	static_assert(parameter_pack_index<char, int, char, bool>::value == 1u, "");

	static_assert(parameter_pack_value<1u, uint64_t, 7u, 9u, 11u>::value == 9u, "");
}

SAW_TEST("Abstract/Templates/KeyPack"){
	using namespace kel::lbm;

	static_assert(parameter_key_pack_index<"b", "a", "b", "c">::value == 1u, "");
	static_assert(parameter_key_pack_type<2u, "a", "b", "c">::literal == string_literal{"c"}, "");
}

SAW_TEST("Abstract/Templates/DigitConversion"){
	using namespace kel::lbm;

	static_assert(ct_convert_to_digits<0u,10u>::literal == string_literal{"0"}, "");
	static_assert(ct_convert_to_digits<7u,10u>::literal == string_literal{"7"}, "");
	static_assert(ct_convert_to_digits<1024u,10u>::literal == string_literal{"1024"}, "");
}

SAW_TEST("Abstract/StringLiteral/Concat"){
	using namespace kel::lbm;

	static_assert((string_literal{"foo"} + string_literal{"bar"}) == string_literal{"foobar"}, "");
	static_assert(string_literal{"foo"}.view() == std::string_view{"foo"}, "");
}

SAW_TEST("Abstract/Schema/Meta"){
	using namespace kel::lbm;

	// Compile time shapes carry nothing at runtime.
	static_assert(std::is_same_v<ts::UInt64::Meta, ts::Void>, "");
	static_assert(std::is_same_v<ts::FixedArray<ts::UInt64,3u>::Meta, ts::Void>, "");

	// A dynamically sized Array is described by its extents.
	static_assert(std::is_same_v<
		ts::Array<ts::Float64,2u>::Meta,
		ts::FixedArray<ts::UInt64,2u>>, "");

	// A String is described by its length.
	static_assert(std::is_same_v<ts::String::Meta, ts::UInt64>, "");

	// A Tensor is addressed through a rank sized index.
	static_assert(std::is_same_v<
		ts::Vector<ts::Float64,3u>::Meta,
		ts::FixedArray<ts::UInt64,1u>>, "");
	static_assert(std::is_same_v<
		ts::Scalar<ts::Float64>::Meta,
		ts::FixedArray<ts::UInt64,0u>>, "");

	// Structural schemas recurse into their members.
	using TStruct = ts::Struct<
		ts::Member<ts::UInt64,"count">,
		ts::Member<ts::Array<ts::Float64,1u>,"values">
	>;
	static_assert(std::is_same_v<
		TStruct::Meta,
		ts::Struct<
			ts::Member<ts::Void,"count">,
			ts::Member<ts::FixedArray<ts::UInt64,1u>,"values">
		>>, "");

	static_assert(std::is_same_v<
		ts::Tuple<ts::UInt64, ts::String>::Meta,
		ts::Tuple<ts::Void, ts::UInt64>>, "");
}

SAW_TEST("Abstract/Schema/PrimitiveShape"){
	using namespace kel::lbm;

	static_assert(ts::UInt64::Bytes == 8u, "");
	static_assert(std::is_same_v<ts::UInt64::Kind, ts::UnsignedInteger>, "");
	static_assert(std::is_same_v<ts::UInt64::StorageType, ts::UInt64>, "");
	static_assert(std::is_same_v<ts::UInt64::InterfaceType, ts::UInt64>, "");

	using TMixed = ts::MixedPrecision<ts::Float32, ts::Float64>;
	static_assert(std::is_same_v<TMixed::StorageType, ts::Float32>, "");
	static_assert(std::is_same_v<TMixed::InterfaceType, ts::Float64>, "");

	static_assert(ts::FixedArray<ts::UInt64,2u,3u>::FlatSize == 6u, "");
	static_assert(ts::FixedArray<ts::UInt64,2u,3u>::Rank == 2u, "");
	static_assert(ts::Scalar<ts::Float64>::FlatSize == 1u, "rank zero holds one element");
}

SAW_TEST("Abstract/SchemaStringify/Primitive"){
	using namespace kel::lbm;

	static_assert(schema_to_string<ts::UInt64>::literal
		== string_literal{"Primitive<UnsignedInteger,8>"}, "");
	static_assert(schema_to_string<ts::Float32>::literal
		== string_literal{"Primitive<FloatingPoint,4>"}, "");
}

SAW_TEST("Abstract/SchemaStringify/Composite"){
	using namespace kel::lbm;

	static_assert(schema_to_string<ts::FixedArray<ts::UInt8,2u,3u>>::literal
		== string_literal{"FixedArray<Primitive<UnsignedInteger,1>,2,3>"}, "");

	static_assert(schema_to_string<ts::Tuple<ts::Bool, ts::String>>::literal
		== string_literal{"Tuple<Bool,String>"}, "");
}

SAW_TEST("Abstract/Error/MakeError"){
	using namespace kel::lbm;

	auto err = make_error<err::not_found>("missing thing");
	SAW_EXPECT(err.failed(), "a not_found is an error");
	SAW_EXPECT(err.is_recoverable(), "not_found is recoverable");
	SAW_EXPECT(err.is_type<err::not_found>(), "error keeps its type");
	SAW_EXPECT(!err.is_type<err::out_of_memory>(), "and is distinguishable");
	SAW_EXPECT(err.get_message() == std::string_view{"missing thing"}, "message survives");
}

SAW_TEST("Abstract/Error/Critical"){
	using namespace kel::lbm;

	auto err = make_error<err::out_of_memory>();
	SAW_EXPECT(err.is_critical(), "out_of_memory is critical");
	SAW_EXPECT(!err.is_recoverable(), "and therefore not recoverable");
}

SAW_TEST("Abstract/Error/ErrorOr"){
	using namespace kel::lbm;

	error_or<uint64_t> good{uint64_t{42u}};
	SAW_EXPECT(good.is_value(), "holds a value");
	SAW_EXPECT(!good.is_error(), "and not an error");
	SAW_EXPECT(good.get_value() == 42u, "value round trips");

	error_or<uint64_t> bad{make_error<err::not_found>()};
	SAW_EXPECT(bad.is_error(), "holds an error");
	SAW_EXPECT(!bad.is_value(), "and not a value");
	SAW_EXPECT(bad.get_error().is_type<err::not_found>(), "error type survives");
}

SAW_TEST("Abstract/Error/VoidPath"){
	using namespace kel::lbm;

	error_or<void> ok{make_void()};
	SAW_EXPECT(ok.is_value(), "make_void is a value, not an error");
}

SAW_TEST("Abstract/Common/Pointers"){
	using namespace kel::lbm;

	auto owned = heap<uint64_t>(7u);
	SAW_EXPECT(*owned == 7u, "heap constructs in place");

	ptr<uint64_t> empty;
	SAW_EXPECT(!empty.is_valid(), "a default ptr is empty");

	uint64_t backing = 3u;
	ptr<uint64_t> filled{backing};
	SAW_EXPECT(filled.is_valid(), "a bound ptr is valid");
	SAW_EXPECT(filled() == 3u, "and reads through");

	ref<uint64_t> r{backing};
	r() = 9u;
	SAW_EXPECT(backing == 9u, "ref writes through to its referent");
}
}
