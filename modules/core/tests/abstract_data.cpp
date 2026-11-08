#include <forstio/test/suite.hpp>

#include "../c++/abstract/data.hpp"
#include "../c++/abstract/data_math.hpp"
#include "../c++/abstract/iterator.hpp"
#include "../c++/abstract/math.hpp"

namespace {
namespace ts {
using namespace kel::lbm::sch;
}

using kel::lbm::data;
using kel::lbm::enc::Native;

SAW_TEST("Abstract/Data/Primitive"){
	data<ts::UInt64> a{4u};
	data<ts::UInt64> b{6u};

	SAW_EXPECT(a.get() == 4u, "value round trips");
	SAW_EXPECT((a + b).get() == 10u, "addition");
	SAW_EXPECT((b - a).get() == 2u, "subtraction");
	SAW_EXPECT((a * b).get() == 24u, "multiplication");
	SAW_EXPECT((b / a).get() == 1u, "integer division");

	a += b;
	SAW_EXPECT(a.get() == 10u, "compound addition mutates");
	a -= b;
	SAW_EXPECT(a.get() == 4u, "compound subtraction mutates");

	SAW_EXPECT(a < b, "ordering");
	SAW_EXPECT(b > a, "reverse ordering");
	SAW_EXPECT(a != b, "inequality");
	SAW_EXPECT(a.equals(data<ts::UInt64>{4u}), "equality");

	++a;
	SAW_EXPECT(a.get() == 5u, "increment");
}

SAW_TEST("Abstract/Data/PrimitiveCast"){
	data<ts::Float64> d{2.75};
	auto as_int = d.cast_to<ts::Int32>();
	SAW_EXPECT(as_int.get() == 2, "float to int truncates");

	data<ts::UInt8> small{255u};
	auto wide = small.cast_to<ts::UInt64>();
	SAW_EXPECT(wide.get() == 255u, "widening preserves the value");
}

SAW_TEST("Abstract/Data/Bool"){
	data<ts::Bool> t{true};
	data<ts::Bool> f{false};

	SAW_EXPECT(t.get(), "true round trips");
	SAW_EXPECT((!t).get() == false, "negation");
	SAW_EXPECT((t && f).get() == false, "conjunction");
	SAW_EXPECT((t || f).get() == true, "disjunction");
	SAW_EXPECT(t != f, "inequality");
}

SAW_TEST("Abstract/Data/MixedPrecision"){
	// Stored as float, read as double.
	using TMixed = ts::MixedPrecision<ts::Float32, ts::Float64>;

	data<TMixed> v{1.5};
	SAW_EXPECT(v.get() == 1.5, "a value exactly representable in float survives");

	data<TMixed> w{2.5};
	SAW_EXPECT((v + w).get() == 4.0, "arithmetic happens at interface precision");

	static_assert(std::is_same_v<decltype(v.get()), double>,
		"reads come back as the interface type");
}

SAW_TEST("Abstract/Data/FixedArrayRankOne"){
	data<ts::FixedArray<ts::UInt64,3u>> arr{1u,2u,3u};

	SAW_EXPECT(arr.at({0u}).get() == 1u, "single index access");
	SAW_EXPECT(arr.at({2u}).get() == 3u, "last element");
	SAW_EXPECT(arr.at({{1u}}).get() == 2u, "index array access");
	SAW_EXPECT(arr.flat_at(1u).get() == 2u, "flat access");

	arr.at({1u}).set(9u);
	SAW_EXPECT(arr.at({1u}).get() == 9u, "writes through");

	SAW_EXPECT(arr.size().get() == 3u, "size");
	SAW_EXPECT(decltype(arr)::get_dim_size<0u>() == 3u, "compile time extent");
}

SAW_TEST("Abstract/Data/FixedArrayRankTwo"){
	data<ts::FixedArray<ts::UInt64,2u,3u>> arr;

	SAW_EXPECT(arr.size().get() == 6u, "flat size is the product of extents");

	// Row major: index {i,j} sits at i + j*2.
	arr.at({{1u,2u}}).set(7u);
	SAW_EXPECT(arr.at({{1u,2u}}).get() == 7u, "two dimensional access");
	SAW_EXPECT(arr.flat_at(1u + 2u*2u).get() == 7u, "and its flat position");

	SAW_EXPECT(decltype(arr)::get_dim_size<0u>() == 2u, "first extent");
	SAW_EXPECT(decltype(arr)::get_dim_size<1u>() == 3u, "second extent");
}

SAW_TEST("Abstract/Data/FixedArrayRankZero"){
	// The empty product is one, so a rank zero shape still holds one element.
	data<ts::FixedArray<ts::Float64>> scalar_like;
	SAW_EXPECT(scalar_like.size().get() == 1u, "rank zero holds exactly one element");

	scalar_like.at({}).set(3.5);
	SAW_EXPECT(scalar_like.at({}).get() == 3.5, "addressed with an empty index");
}

SAW_TEST("Abstract/Data/Array"){
	// Extents are runtime, so they come in as the metadata.
	data<ts::Array<ts::UInt64,1u>> arr{data<ts::FixedArray<ts::UInt64,1u>>{{3u}}};

	SAW_EXPECT(arr.size().get() == 3u, "sized from its metadata");
	SAW_EXPECT(arr.get_dim_size(0u) == 3u, "extent is readable");

	arr.at({1u}).set(5u);
	SAW_EXPECT(arr.at({1u}).get() == 5u, "single index access");
	SAW_EXPECT(arr.at({{1u}}).get() == 5u, "index array access");

	auto eov = arr.add(data<ts::UInt64>{8u});
	SAW_EXPECT(eov.is_value(), "appending to the last dimension works");
	SAW_EXPECT(arr.size().get() == 4u, "and grows it");
	SAW_EXPECT(arr.at({3u}).get() == 8u, "appended value lands last");
}

SAW_TEST("Abstract/Data/ArrayFromIntegralExtents"){
	data<ts::Array<ts::UInt64,2u>> arr{2u, 3u};

	SAW_EXPECT(arr.size().get() == 6u, "extents multiply out");
	arr.at({{1u,2u}}).set(4u);
	SAW_EXPECT(arr.at({{1u,2u}}).get() == 4u, "two dimensional access");
}

SAW_TEST("Abstract/Data/String"){
	data<ts::String> s{std::string{"hello"}};

	SAW_EXPECT(s.size().get() == 5u, "length");
	SAW_EXPECT(s.get_at({1u}) == 'e', "indexed read");

	s.set_at({0u}, 'H');
	SAW_EXPECT(s.stl_view() == std::string_view{"Hello"}, "indexed write");

	// A String is described by its length, so building one from metadata
	// gives a string of that length.
	data<ts::String> sized{data<ts::UInt64>{3u}};
	SAW_EXPECT(sized.size().get() == 3u, "sized from its metadata");
}

SAW_TEST("Abstract/Data/Tuple"){
	data<ts::Tuple<ts::UInt64, ts::Bool>> t;

	t.get<0u>().set(7u);
	t.get<1u>().set(true);

	SAW_EXPECT(t.get<0u>().get() == 7u, "first member");
	SAW_EXPECT(t.get<1u>().get() == true, "second member");
	SAW_EXPECT(decltype(t)::size() == 2u, "member count");
}

SAW_TEST("Abstract/Data/Struct"){
	using TStruct = ts::Struct<
		ts::Member<ts::UInt64,"count">,
		ts::Member<ts::Float64,"weight">
	>;

	data<TStruct> s;
	s.get<"count">().set(12u);
	s.get<"weight">().set(0.25);

	SAW_EXPECT(s.get<"count">().get() == 12u, "member by name");
	SAW_EXPECT(s.get<"weight">().get() == 0.25, "second member by name");
	SAW_EXPECT(s.get_at_index<0u>().get() == 12u, "and by position");
	SAW_EXPECT(decltype(s)::size().get() == 2u, "member count");
}

SAW_TEST("Abstract/Data/Union"){
	using TUnion = ts::Union<
		ts::Member<ts::UInt64,"number">,
		ts::Member<ts::Bool,"flag">
	>;

	data<TUnion> u;
	u.init<"number">().set(3u);

	SAW_EXPECT(u.holds_alternative<"number">(), "holds the initialised member");
	SAW_EXPECT(!u.holds_alternative<"flag">(), "and not the other one");
	SAW_EXPECT(u.get<"number">().get() == 3u, "value reads back");

	u.init<"flag">().set(true);
	SAW_EXPECT(u.holds_alternative<"flag">(), "switching alternative");
	SAW_EXPECT(u.get<"flag">().get() == true, "new value reads back");
}

SAW_TEST("Abstract/Data/Tensor"){
	data<ts::Vector<ts::Float64,3u>> a{1.0, 2.0, 3.0};
	data<ts::Vector<ts::Float64,3u>> b{4.0, 5.0, 6.0};

	SAW_EXPECT(a.at({{0u}}).get() == 1.0, "component access");
	SAW_EXPECT(a.flat_at(2u).get() == 3.0, "flat access");

	auto sum = a + b;
	SAW_EXPECT(sum.flat_at(0u).get() == 5.0, "componentwise addition");
	SAW_EXPECT(sum.flat_at(2u).get() == 9.0, "componentwise addition, last");

	auto diff = b - a;
	SAW_EXPECT(diff.flat_at(1u).get() == 3.0, "componentwise subtraction");

	auto neg = -a;
	SAW_EXPECT(neg.flat_at(0u).get() == -1.0, "negation");
}

SAW_TEST("Abstract/Data/TensorScalarOps"){
	data<ts::Vector<ts::Float64,2u>> v{3.0, 4.0};
	auto two = kel::lbm::math::make_scalar<ts::Float64>(2.0);

	auto scaled = v * two;
	SAW_EXPECT(scaled.flat_at(0u).get() == 6.0, "scalar multiply");

	auto halved = v / two;
	SAW_EXPECT(halved.flat_at(1u).get() == 2.0, "scalar divide");
}

SAW_TEST("Abstract/Data/ScalarTensor"){
	// Rank zero: one element, addressed with an empty index.
	data<ts::Scalar<ts::Float64>> s{data<ts::Float64>{2.5}};

	SAW_EXPECT(s.at({}).get() == 2.5, "empty index access");
	SAW_EXPECT(s.flat_at(0u).get() == 2.5, "flat access");
	SAW_EXPECT(decltype(s)::size().get() == 1u, "holds one element");
}

SAW_TEST("Abstract/Math/DotAndNorm"){
	using namespace kel::lbm;

	auto a = math::make_vector<ts::Float64>(3.0, 4.0);
	auto b = math::make_vector<ts::Float64>(1.0, 0.0);

	auto d = math::dot<ts::Float64,2u>(a, b);
	SAW_EXPECT(d.flat_at(0u).get() == 3.0, "dot product");

	auto n = math::norm<ts::Float64,2u>(a);
	SAW_EXPECT(n.flat_at(0u).get() == 5.0, "3-4-5 triangle");
}

SAW_TEST("Abstract/Math/Normalize"){
	using namespace kel::lbm;

	auto a = math::make_vector<ts::Float64>(0.0, 2.0);
	auto u = math::normalize<ts::Float64,2u>(a);

	SAW_EXPECT(u.flat_at(0u).get() == 0.0, "x component");
	SAW_EXPECT(u.flat_at(1u).get() == 1.0, "y component is unit length");

	// A zero vector has no direction to preserve, so it comes back unchanged
	// rather than dividing by zero.
	auto zero = math::make_vector<ts::Float64>(0.0, 0.0);
	auto uz = math::normalize<ts::Float64,2u>(zero);
	SAW_EXPECT(uz.flat_at(0u).get() == 0.0, "zero vector survives untouched");
}

SAW_TEST("Abstract/Math/Cross"){
	using namespace kel::lbm;

	auto x = math::make_vector<ts::Float64>(1.0, 0.0, 0.0);
	auto y = math::make_vector<ts::Float64>(0.0, 1.0, 0.0);

	auto z = math::cross<ts::Float64>(x, y);
	SAW_EXPECT(z.flat_at(2u).get() == 1.0, "x cross y is z");

	// In two dimensions only the normal component survives.
	auto a = math::make_vector<ts::Float64>(1.0, 0.0);
	auto b = math::make_vector<ts::Float64>(0.0, 1.0);
	auto s = math::cross<ts::Float64>(a, b);
	SAW_EXPECT(s.flat_at(0u).get() == 1.0, "2d cross is a scalar");
}

SAW_TEST("Abstract/Math/Vectorize"){
	using namespace kel::lbm;

	data<ts::FixedArray<ts::Float64,3u>> arr{1.0, 2.0, 3.0};
	auto v = math::vectorize_data<ts::Float64,3u>(arr);

	SAW_EXPECT(v.flat_at(0u).get() == 1.0, "first component carried over");
	SAW_EXPECT(v.flat_at(2u).get() == 3.0, "last component carried over");
}

SAW_TEST("Abstract/Math/MatrixVectorMultiply"){
	using namespace kel::lbm;

	// Column major by the row-major-index convention used in get_flat_index:
	// at({{i,j}}) is element i of column j.
	data<ts::Matrix<ts::Float64,2u,2u>> m;
	m.at({{0u,0u}}).set(1.0);
	m.at({{0u,1u}}).set(2.0);
	m.at({{1u,0u}}).set(3.0);
	m.at({{1u,1u}}).set(4.0);

	auto v = math::make_vector<ts::Float64>(1.0, 1.0);
	auto r = math::multiply<ts::Float64,2u,2u>(m, v);

	SAW_EXPECT(r.flat_at(0u).get() == 3.0, "first row sums to 3");
	SAW_EXPECT(r.flat_at(1u).get() == 7.0, "second row sums to 7");
}

SAW_TEST("Abstract/Math/SumAndFill"){
	using namespace kel::lbm;

	data<ts::Vector<ts::Float64,3u>> v;
	math::fill<ts::Float64,3u>(v, data<ts::Float64>{2.0});

	SAW_EXPECT(v.flat_at(1u).get() == 2.0, "fill writes every element");

	auto s = math::sum<ts::Float64,3u>(v);
	SAW_EXPECT(s.flat_at(0u).get() == 6.0, "sum over the tensor");
}

SAW_TEST("Abstract/Iterator/RankOne"){
	using namespace kel::lbm;

	uint64_t visits = 0u;
	auto eov = rank_iterator<3u>::in_fixed_bounds(
		[&](const index_data<1u>& index) -> error_or<void> {
			SAW_EXPECT(index.flat_at(0u).get() == visits, "visited in order");
			++visits;
			return make_void();
		});

	SAW_EXPECT(eov.is_value(), "iteration succeeded");
	SAW_EXPECT(visits == 3u, "visited every position");
}

SAW_TEST("Abstract/Iterator/RankTwo"){
	using namespace kel::lbm;

	// This is the case that could not compile against forstio's iterator,
	// because it indexed a rank two index array with a bare integer.
	uint64_t visits = 0u;
	uint64_t last_i = 0u;
	uint64_t last_j = 0u;

	auto eov = rank_iterator<2u,3u>::in_fixed_bounds(
		[&](const index_data<2u>& index) -> error_or<void> {
			last_i = index.flat_at(0u).get();
			last_j = index.flat_at(1u).get();
			++visits;
			return make_void();
		});

	SAW_EXPECT(eov.is_value(), "iteration succeeded");
	SAW_EXPECT(visits == 6u, "visited every position of a 2x3 shape");
	SAW_EXPECT(last_i == 1u, "finished on the last i");
	SAW_EXPECT(last_j == 2u, "finished on the last j");
}

SAW_TEST("Abstract/Iterator/RankZero"){
	using namespace kel::lbm;

	uint64_t visits = 0u;
	auto eov = rank_iterator<>::in_fixed_bounds(
		[&](const index_data<0u>&) -> error_or<void> {
			++visits;
			return make_void();
		});

	SAW_EXPECT(eov.is_value(), "iteration succeeded");
	SAW_EXPECT(visits == 1u, "rank zero still has one position");
}

SAW_TEST("Abstract/Iterator/PropagatesError"){
	using namespace kel::lbm;

	uint64_t visits = 0u;
	auto eov = rank_iterator<5u>::in_fixed_bounds(
		[&](const index_data<1u>&) -> error_or<void> {
			++visits;
			if(visits == 2u){
				return make_error<err::not_found>();
			}
			return make_void();
		});

	SAW_EXPECT(eov.is_error(), "the error reaches the caller");
	SAW_EXPECT(eov.get_error().is_type<err::not_found>(), "and keeps its type");
	SAW_EXPECT(visits == 2u, "iteration stopped at the failure");
}
}
