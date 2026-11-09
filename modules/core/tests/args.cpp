#include <forstio/test/suite.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "../c++/abstract/args.hpp"
#include "../c++/lbm_unit.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

/**
 * argv is char**, so the fixture has to own the strings and hand out pointers.
 */
struct Argv {
	std::vector<std::string> store;
	std::vector<char*> ptrs;

	explicit Argv(std::vector<std::string> words): store{std::move(words)} {
		ptrs.reserve(store.size());
		for(auto& s : store){
			ptrs.push_back(s.data());
		}
	}

	int argc() const { return static_cast<int>(ptrs.size()); }
	char** argv() { return ptrs.data(); }
};

SAW_TEST("Args / named and positional round trip"){
	using namespace kel;

	using ArgStruct = sch::Struct<
		sch::Member<sch::Float64, "dx">,
		sch::Member<sch::Float64, "dt">
	>;
	using Sch = sch::Args<ArgStruct, sch::Tuple<sch::UInt64, sch::UInt64>>;

	Argv a{{"prog","--dx","0.25","2","--dt","0.5","9"}};

	kel::lbm::data<Sch> parsed;
	auto eov = lbm::parse_args(parsed, a.argc(), a.argv());
	SAW_EXPECT(eov.is_value(), "parse should succeed");

	SAW_EXPECT(parsed.template get<"program">().stl_view() == "prog", "argv[0] is the program");

	auto& args = parsed.template get<"args">();
	SAW_EXPECT(args.template get<"dx">().get() == 0.25, "dx");
	SAW_EXPECT(args.template get<"dt">().get() == 0.5, "dt");

	// Positionals keep their order even when interleaved with named arguments.
	auto& pos = parsed.template get<"positionals">();
	SAW_EXPECT(pos.template get<0u>().get() == 2u, "first positional");
	SAW_EXPECT(pos.template get<1u>().get() == 9u, "second positional");
}

SAW_TEST("Args / reports the unknown flag by name"){
	using namespace kel;

	using Sch = sch::Args<
		sch::Struct<sch::Member<sch::Float64, "dx">>,
		sch::Tuple<>
	>;

	Argv a{{"prog","--nope","1"}};

	kel::lbm::data<Sch> parsed;
	auto eov = lbm::parse_args(parsed, a.argc(), a.argv());

	SAW_EXPECT(eov.is_error(), "an unknown flag must be an error");
	// Naming it is the difference between a puzzled re-run and a fix.
	SAW_EXPECT(eov.get_error().get_message().find("nope") != std::string_view::npos,
		std::string{"error should name the flag: "}
		+ std::string{eov.get_error().get_message()});
}

SAW_TEST("Args / rejects malformed values and arity mistakes"){
	using namespace kel;

	using Sch = sch::Args<
		sch::Struct<sch::Member<sch::Float64, "dx">>,
		sch::Tuple<sch::UInt64>
	>;

	{
		Argv a{{"prog","--dx","abc","1"}};
		kel::lbm::data<Sch> p;
		SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_error(),
			"a non numeric value must fail");
	}
	{
		Argv a{{"prog","--dx"}};
		kel::lbm::data<Sch> p;
		SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_error(),
			"a flag with no value must fail");
	}
	{
		Argv a{{"prog","--dx","1.0"}};
		kel::lbm::data<Sch> p;
		SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_error(),
			"a missing positional must fail");
	}
	{
		Argv a{{"prog","--dx","1.0","1","2"}};
		kel::lbm::data<Sch> p;
		SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_error(),
			"a surplus positional must fail");
	}
	{
		// Trailing text after a number is a mistake, not a prefix to accept.
		Argv a{{"prog","--dx","1.0x","1"}};
		kel::lbm::data<Sch> p;
		SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_error(),
			"trailing junk must fail rather than parse the prefix");
	}
}

SAW_TEST("Args / integers reject fractional input"){
	using namespace kel;

	using Sch = sch::Args<
		sch::Struct<sch::Member<sch::UInt64, "count">>,
		sch::Tuple<>
	>;

	Argv good{{"prog","--count","42"}};
	kel::lbm::data<Sch> p;
	SAW_EXPECT(lbm::parse_args(p, good.argc(), good.argv()).is_value(), "42 parses");
	SAW_EXPECT(p.template get<"args">().template get<"count">().get() == 42u, "count");

	// 1.5 is not an integer; truncating silently would lose data.
	Argv bad{{"prog","--count","1.5"}};
	kel::lbm::data<Sch> q;
	SAW_EXPECT(lbm::parse_args(q, bad.argc(), bad.argv()).is_error(),
		"a fractional value is not an integer");
}

SAW_TEST("Args / strings and bools"){
	using namespace kel;

	using Sch = sch::Args<
		sch::Struct<
			sch::Member<sch::String, "coupling">,
			sch::Member<sch::Bool, "verbose">
		>,
		sch::Tuple<>
	>;

	Argv a{{"prog","--coupling","fplbm","--verbose","true"}};
	kel::lbm::data<Sch> p;
	SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_value(), "parse should succeed");

	auto& args = p.template get<"args">();
	SAW_EXPECT(args.template get<"coupling">().stl_view() == "fplbm", "string member");
	SAW_EXPECT(args.template get<"verbose">().get() == true, "bool member");

	for(const char* word : {"false","0","no","off"}){
		Argv b{{"prog","--coupling","x","--verbose",word}};
		kel::lbm::data<Sch> q;
		SAW_EXPECT(lbm::parse_args(q, b.argc(), b.argv()).is_value(),
			std::string{"'"} + word + "' should parse");
		SAW_EXPECT(q.template get<"args">().template get<"verbose">().get() == false,
			std::string{"'"} + word + "' should be false");
	}

	Argv bad{{"prog","--coupling","x","--verbose","maybe"}};
	kel::lbm::data<Sch> r;
	SAW_EXPECT(lbm::parse_args(r, bad.argc(), bad.argv()).is_error(),
		"a non boolean must fail rather than default");
}

SAW_TEST("Args / mixed precision keeps its interface type"){
	using namespace kel;

	/**
	 * Stored narrow, read wide. The command line value goes in at interface
	 * precision, which is the whole reason for parsing through the schema
	 * instead of into a bare double.
	 */
	using Mixed = sch::MixedPrecision<sch::Float32, sch::Float64>;
	using Sch = sch::Args<
		sch::Struct<sch::Member<Mixed, "visc">>,
		sch::Tuple<>
	>;

	Argv a{{"prog","--visc","0.5"}};
	kel::lbm::data<Sch> p;
	SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_value(), "parse should succeed");

	auto& v = p.template get<"args">().template get<"visc">();
	static_assert(std::is_same_v<decltype(v.get()), double>,
		"a mixed precision read must come back at interface precision");
	SAW_EXPECT(v.get() == 0.5, "value exactly representable in float survives");
}

SAW_TEST("Args / united quantities stay united"){
	using namespace kel;

	/**
	 * The unit lives in the schema, not the text, so nothing extra is read --
	 * but the parsed result is typed, so a length cannot be passed where a
	 * time is wanted.
	 */
	using Sch = sch::Args<
		sch::Struct<
			sch::Member<sch::SiMeter<sch::Float64>, "length">,
			sch::Member<sch::SiSecond<sch::Float64>, "duration">
		>,
		sch::Tuple<>
	>;

	Argv a{{"prog","--length","2.5","--duration","0.1"}};
	kel::lbm::data<Sch> p;
	SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_value(), "parse should succeed");

	auto& args = p.template get<"args">();
	SAW_EXPECT(args.template get<"length">().handle().get() == 2.5, "length value");
	SAW_EXPECT(args.template get<"duration">().handle().get() == 0.1, "duration value");

	// The two are different types, so the unit survived parsing.
	static_assert(!std::is_same_v<
			std::decay_t<decltype(args.template get<"length">())>,
			std::decay_t<decltype(args.template get<"duration">())>>,
		"a length and a time must not share a type after parsing");

	// And the algebra still works on what came off the command line.
	auto velocity = args.template get<"length">() / args.template get<"duration">();
	SAW_EXPECT(std::abs(velocity.handle().get() - 25.0) < 1e-9,
		std::string{"2.5 m / 0.1 s should be 25: "}
		+ std::to_string(velocity.handle().get()));
}

SAW_TEST("Args / scalars parse as single values"){
	using namespace kel;

	// Scalar<T> is a rank zero tensor; it should take one number, not a list.
	using Sch = sch::Args<
		sch::Struct<sch::Member<sch::Scalar<sch::Float64>, "radius">>,
		sch::Tuple<>
	>;

	Argv a{{"prog","--radius","3.75"}};
	kel::lbm::data<Sch> p;
	SAW_EXPECT(lbm::parse_args(p, a.argc(), a.argv()).is_value(), "parse should succeed");
	SAW_EXPECT(p.template get<"args">().template get<"radius">().at({}).get() == 3.75,
		"scalar value");
}

SAW_TEST("Args / empty argv is an error not a default"){
	using namespace kel;

	using Sch = sch::Args<sch::Struct<>, sch::Tuple<>>;

	kel::lbm::data<Sch> p;
	auto eov = lbm::parse_args(p, 0, nullptr);
	SAW_EXPECT(eov.is_error(), "no argv at all must be reported");
}
}
