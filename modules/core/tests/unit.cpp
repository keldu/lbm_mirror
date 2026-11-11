#include <forstio/test/suite.hpp>

#include <sstream>
#include <type_traits>

#include "../c++/abstract/unit.hpp"
#include "../c++/abstract/unit_print.hpp"
#include "../c++/lbm_unit.hpp"
#include "../c++/particle/schema.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

using kel::lbm::si_type::meter;
using kel::lbm::si_type::second;
using lbm_meter = kel::lbm::lbm_type::meter;

template <int64_t E, int64_t S = 0>
using Met = sch::UnitElement<meter, E, S>;

template <int64_t E, int64_t S = 0>
using Sec = sch::UnitElement<second, E, S>;

template <typename... Ele> using U = sch::Unit<sch::T, Ele...>;

template <typename A, typename B>
using Mul = typename kel::lbm::unit_multiplication<A, B>::Schema;

template <typename A, typename B>
using Div = typename kel::lbm::unit_division<A, B>::Schema;

/**
 * The unit algebra had no direct coverage: converter.cpp asserts one
 * conversion and args.cpp exercises unit_division once. Nothing pinned
 * multiplication, exponent cancellation, or the scaling comparison -- and the
 * scaling path could not even be instantiated, because unit_multiplication,
 * unit_division and the three print patterns all matched the two argument
 * form, which means scale 0 after the default argument is substituted.
 */

SAW_TEST("Unit / multiplication folds same base and same scale"){
	static_assert(std::is_same_v<Mul<U<Met<1>>, U<Met<1>>>, U<Met<2>>>,
		"m * m must fold to m^2");
	static_assert(std::is_same_v<Mul<U<Met<1,3>>, U<Met<1,3>>>, U<Met<2,3>>>,
		"km * km must fold to km^2, keeping the scale");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / different scales do not fold"){
	// Same base quantity, different prefix: the factors must both survive
	// rather than silently combining, because nothing rescales the value.
	using Mixed = Mul<U<Met<1,0>>, U<Met<1,3>>>;
	static_assert(!std::is_same_v<Mixed, U<Met<2,0>>>, "must not fold to m^2");
	static_assert(!std::is_same_v<Mixed, U<Met<2,3>>>, "must not fold to km^2");
	static_assert(std::is_same_v<Mixed, U<Met<1,0>, Met<1,3>>>,
		"both factors must survive, in order");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / division negates the exponent but keeps the scale"){
	// (km)^-1 is scale 3 exponent -1. Negating the scale too would quietly
	// mean (mm)^-1.
	static_assert(std::is_same_v<Div<U<Met<2,3>>, U<Met<1,3>>>, U<Met<1,3>>>,
		"km^2 / km must be km");
	static_assert(std::is_same_v<Div<U<>, U<Met<1,3>>>, U<Met<-1,3>>>,
		"1 / km must be km^-1, not mm^-1");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / cancelling exponents drop out entirely"){
	// m/m must be genuinely dimensionless, not m^0.
	static_assert(std::is_same_v<Div<U<Met<1>>, U<Met<1>>>, sch::Pure<sch::T>>,
		"m / m must reduce to Pure");
	static_assert(std::is_same_v<Div<U<Met<1,3>>, U<Met<1,3>>>, sch::Pure<sch::T>>,
		"km / km must reduce to Pure");
	// But only when the scales agree.
	static_assert(!std::is_same_v<Div<U<Met<1,3>>, U<Met<1,0>>>, sch::Pure<sch::T>>,
		"km / m must not cancel");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / lattice and SI metres never fold"){
	// This is what actually separates lattice from SI -- distinct base tag
	// types, not the scale.
	using LbmMet = sch::UnitElement<lbm_meter, 1>;
	using Mixed = Mul<U<Met<1>>, U<LbmMet>>;
	static_assert(!std::is_same_v<Mixed, U<Met<2>>>, "must not fold");
	// Canonical order, not encounter order: "meter_lbm" sorts before "meter_si".
	static_assert(std::is_same_v<Mixed, U<LbmMet, Met<1>>>, "both survive, sorted");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / existing aliases are unchanged"){
	// Regression pin: widening the patterns must not have altered any
	// composition that lbm_unit.hpp already produced.
	static_assert(std::is_same_v<sch::SiMeter<sch::T>, U<Met<1>>>, "SiMeter");
	static_assert(std::is_same_v<sch::SiVelocity<sch::T>, U<Met<1>, Sec<-1>>>,
		"SiVelocity");
	static_assert(
		std::is_same_v<sch::SiKinematicViscosity<sch::T>, U<Met<2>, Sec<-1>>>,
		"SiKinematicViscosity");
	static_assert(
		std::is_same_v<Div<sch::SiMeter<sch::T>, sch::SiSecond<sch::T>>,
					   sch::SiVelocity<sch::T>>,
		"m / s must be the velocity alias exactly");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / a scaled unit prints with its prefix"){
	std::stringstream plain, kilo, milli;

	kel::lbm::data<U<Met<1>>> p{2.0};
	kel::lbm::data<U<Met<1,3>>> k{2.0};
	kel::lbm::data<U<Met<1,-3>>> m{2.0};

	plain << p;
	kilo << k;
	milli << m;

	SAW_EXPECT(plain.str() == "2 [m_si]", "unscaled prints bare");
	SAW_EXPECT(kilo.str() == "2 [km_si]", "kilo prints a k prefix");
	SAW_EXPECT(milli.str() == "2 [mm_si]", "milli prints an m prefix");
	// The whole point: a scaled unit must not render identically to an
	// unscaled one, or the output lies about the value.
	SAW_EXPECT(plain.str() != kilo.str(), "scaled must differ from unscaled");
}

SAW_TEST("Unit / an off table scale prints explicitly"){
	std::stringstream o;
	kel::lbm::data<U<Met<1,4>>> odd{2.0};
	o << odd;
	// No SI letter exists for 10^4, so it must say so rather than drop it.
	SAW_EXPECT(o.str() == "2 [10^(4)*m_si]", "unknown scale prints as a power");
}
}

namespace {
/**
 * The particle position is unit typed. These pin the two things that are easy
 * to break without noticing.
 */
SAW_TEST("Unit / a united wrapper is layout compatible with its base"){
	// The whole device path depends on this and nothing else checks it: the
	// particle group crosses to the GPU through sycl_copy_helper<FixedArray>,
	// which is a bulk h.copy of the element block and never recurses into the
	// element schema. A unit wrapper that changed the size would corrupt every
	// particle silently rather than failing to compile.
	using Raw = kel::lbm::data<sch::Vector<sch::T,3u>>;
	using United = kel::lbm::data<sch::LbmMeter<sch::Vector<sch::T,3u>>>;

	static_assert(sizeof(Raw) == sizeof(United),
		"A unit wrapper must not change the layout");
	static_assert(std::is_trivially_copyable_v<United>,
		"A unit wrapper must stay memcpy-able");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / particle position carries lattice metres"){
	kel::lbm::data<sch::ParticleRigidBody<sch::T,3u>> rb;

	auto &pos = rb.template get<"position">();
	auto &pos_old = rb.template get<"position_old">();
	auto &acc = rb.template get<"acceleration">();

	static_assert(
		std::is_same_v<std::remove_cvref_t<decltype(pos)>,
					   kel::lbm::data<sch::LbmMeter<sch::Vector<sch::T,3u>>>>,
		"position must be a lattice length");
	static_assert(
		std::is_same_v<std::remove_cvref_t<decltype(pos_old)>,
					   std::remove_cvref_t<decltype(pos)>>,
		"position_old must match position");
	static_assert(
		std::is_same_v<std::remove_cvref_t<decltype(acc)>,
					   kel::lbm::data<sch::LbmAcceleration<sch::Vector<sch::T,3u>>>>,
		"acceleration must be a lattice acceleration");

	// Element access still reaches the raw component.
	pos.at({{0u}}).set(3.0);
	SAW_EXPECT(pos.at({{0u}}).get() == 3.0, "element access must still work");

	// A difference of two lengths is a length; its dot product is an area.
	auto delta = pos - pos_old;
	static_assert(std::is_same_v<decltype(delta), std::remove_cvref_t<decltype(pos)>>,
		"length minus length is a length");

	auto area = kel::lbm::math::dot(delta, delta);
	static_assert(
		std::is_same_v<decltype(area),
					   kel::lbm::data<sch::Unit<sch::Scalar<sch::T>,
												sch::UnitElement<kel::lbm::lbm_type::meter,2>>>>,
		"dot of two lengths must be an area");
	SAW_EXPECT(area.at({}).get() == 9.0, "and the value must be right");
}
}

namespace {
/**
 * Commutativity at the type level.
 *
 * m * s and s * m are the same physical unit. If the reduction preserves
 * encounter order rather than imposing a canonical one, they become distinct
 * C++ types and the same quantity reached two different ways stops matching.
 */
SAW_TEST("Unit / multiplication is commutative in the type"){
	using A = Mul<U<Met<1>>, U<Sec<-1>>>;
	using B = Mul<U<Sec<-1>>, U<Met<1>>>;
	static_assert(std::is_same_v<A,B>, "m*s and s*m must be the same type");
	SAW_EXPECT(true, "static assert carries this one");
}
}

namespace {
/**
 * An alias written by hand must already be in canonical order, or it will not
 * match the same unit arrived at by arithmetic.
 *
 * Multiplying by a dimensionless unit runs the alias back through the
 * reduction and the sort, so if its factors were declared out of order the
 * round trip changes the type and this fails.
 */
template <typename Alias>
inline constexpr bool is_canonical =
	std::is_same_v<Mul<U<>, Alias>, Alias>;

SAW_TEST("Unit / division is commutative in the type too"){
	// a/b reached two ways must agree.
	using A = Div<Mul<U<Met<1>>, U<Sec<1>>>, U<Sec<2>>>;
	using B = Div<Mul<U<Sec<1>>, U<Met<1>>>, U<Sec<2>>>;
	static_assert(std::is_same_v<A,B>, "operand order must not change the type");
	SAW_EXPECT(true, "static assert carries this one");
}

SAW_TEST("Unit / three factors sort canonically whatever the order"){
	using Kv = sch::UnitElement<kel::lbm::lbm_type::meter, 2>;
	using Sl = sch::UnitElement<kel::lbm::lbm_type::second, -1>;

	using One = Mul<Mul<U<Met<1>>, U<Kv>>, U<Sl>>;
	using Two = Mul<Mul<U<Sl>, U<Met<1>>>, U<Kv>>;
	using Three = Mul<Mul<U<Kv>, U<Sl>>, U<Met<1>>>;

	static_assert(std::is_same_v<One,Two>, "any permutation must agree");
	static_assert(std::is_same_v<Two,Three>, "any permutation must agree");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / every lbm_unit alias is already canonical"){
	static_assert(is_canonical<sch::SiMeter<sch::T>>, "SiMeter");
	static_assert(is_canonical<sch::LbmMeter<sch::T>>, "LbmMeter");
	static_assert(is_canonical<sch::SiSecond<sch::T>>, "SiSecond");
	static_assert(is_canonical<sch::LbmSecond<sch::T>>, "LbmSecond");
	static_assert(is_canonical<sch::SiVelocity<sch::T>>, "SiVelocity");
	static_assert(is_canonical<sch::LbmVelocity<sch::T>>, "LbmVelocity");
	static_assert(is_canonical<sch::SiAcceleration<sch::T>>, "SiAcceleration");
	static_assert(is_canonical<sch::LbmAcceleration<sch::T>>, "LbmAcceleration");
	static_assert(is_canonical<sch::SiKinematicViscosity<sch::T>>, "SiKinematicViscosity");
	static_assert(is_canonical<sch::LbmKinematicViscosity<sch::T>>, "LbmKinematicViscosity");
	SAW_EXPECT(true, "static asserts carry this one");
}
}

namespace {
/**
 * Velocity is typed through the kinematics and unwrapped at the force law,
 * because force needs a mass base unit that does not exist yet.
 */
SAW_TEST("Unit / a length over a time is a velocity"){
	using Vel = Div<sch::LbmMeter<sch::T>, sch::LbmSecond<sch::T>>;
	static_assert(std::is_same_v<Vel, sch::LbmVelocity<sch::T>>,
		"m / s must be exactly the LbmVelocity alias");

	// And a velocity times a time is back to a length.
	using Len = Mul<sch::LbmVelocity<sch::T>, sch::LbmSecond<sch::T>>;
	static_assert(std::is_same_v<Len, sch::LbmMeter<sch::T>>,
		"velocity * time must be a length again");
	SAW_EXPECT(true, "static asserts carry this one");
}

SAW_TEST("Unit / a normalised vector is Pure, not merely unannotated"){
	kel::lbm::data<sch::LbmMeter<sch::Vector<sch::T,3u>>> v;
	v.at({{0u}}).set(3.0);
	v.at({{1u}}).set(4.0);
	v.at({{2u}}).set(0.0);

	auto dir = kel::lbm::math::normalize(v);
	static_assert(
		std::is_same_v<decltype(dir),
					   kel::lbm::data<sch::Pure<sch::Vector<sch::T,3u>>>>,
		"a direction must be Pure");

	SAW_EXPECT(std::abs(dir.at({{0u}}).get() - 0.6) < 1e-12, "x component");
	SAW_EXPECT(std::abs(dir.at({{1u}}).get() - 0.8) < 1e-12, "y component");

	// The length keeps the unit it measured.
	auto len = kel::lbm::math::norm(v);
	static_assert(std::is_same_v<decltype(len), kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>>>,
		"a length must stay a length");
	SAW_EXPECT(std::abs(len.at({}).get() - 5.0) < 1e-12, "norm value");
}
}

namespace {
/**
 * sqrt halves the exponents, so the root of an area is a length. This is what
 * lets `pos_rel / sqrt(distance_squared)` come out as a dimensionless
 * direction rather than an unlabelled vector.
 */
SAW_TEST("Unit / sqrt of an area is a length"){
	using Area = Mul<sch::LbmMeter<sch::Scalar<sch::T>>, sch::LbmMeter<sch::Scalar<sch::T>>>;

	kel::lbm::data<Area> a;
	a.at({}).set(9.0);

	auto len = kel::lbm::math::sqrt(a);
	static_assert(
		std::is_same_v<decltype(len), kel::lbm::data<sch::LbmMeter<sch::Scalar<sch::T>>>>,
		"sqrt(m^2) must be m");
	SAW_EXPECT(std::abs(len.at({}).get() - 3.0) < 1e-12, "and the value must be right");

	// A length divided by a length is a direction scale: dimensionless.
	using Ratio = Div<sch::LbmMeter<sch::Scalar<sch::T>>, sch::LbmMeter<sch::Scalar<sch::T>>>;
	static_assert(std::is_same_v<Ratio, sch::Pure<sch::Scalar<sch::T>>>,
		"m / m must be Pure");
	SAW_EXPECT(true, "static asserts carry the rest");
}

SAW_TEST("Unit / sqrt refuses an odd exponent"){
	// m^3 has no expressible square root in this system. The static_assert in
	// math::sqrt is what stops it, so this test records the intent rather
	// than invoking it -- instantiating it would fail the build by design.
	using Cube = Mul<Mul<sch::LbmMeter<sch::Scalar<sch::T>>,
						 sch::LbmMeter<sch::Scalar<sch::T>>>,
					 sch::LbmMeter<sch::Scalar<sch::T>>>;
	static_assert(std::is_same_v<Cube,
		kel::lbm::sch::Unit<sch::Scalar<sch::T>,
							sch::UnitElement<kel::lbm::lbm_type::meter,3>>>,
		"m*m*m must be m^3");
	SAW_EXPECT(true, "sqrt of this would not compile, by design");
}
}
