#pragma once

#include <type_traits>

#include "data.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {
namespace sch {

/**
 * One factor of a unit: a base quantity raised to `Expo`, at decimal scale
 * `Scal`.
 *
 * `Scal` is an SI prefix exponent -- nano -9, micro -6, milli -3, none 0, kilo
 * 3, mega 6, giga 9. It is a pure tag: the stored number is the value as
 * written, so 2 km holds 2.0 and nothing is ever multiplied by a power of ten.
 *
 * It is NOT what separates lattice from SI. That comes from the base tag type
 * itself -- `lbm_type::meter` and `si_type::meter` are distinct types, so
 * mixing them cannot compile regardless of scale (see lbm_unit.hpp).
 *
 * Two factors fold their exponents only when base AND scale match
 * (unit_matching_reduce below). Differently scaled quantities therefore do not
 * add: km + m is a compile error by type identity. Implicit rescaling is
 * deliberately absent -- add it here if it is ever wanted.
 */
template <typename Un, int64_t Expo = 0, int64_t Scal = 0> struct UnitElement {
	using Schema = Un;
	static constexpr int64_t Exponent = Expo;
	static constexpr int64_t Scaling = Scal;
	static constexpr string_literal name = "UnitElement";
};

/**
 * A value of `BaseSchema` carrying a unit made of `Components`.
 */
template <typename BaseSchema, typename... Components> struct Unit {
	using Meta = typename BaseSchema::Meta;
	using Schema = BaseSchema;
	static constexpr string_literal name = "Unit";
};

/**
 * A dimensionless value: a unit with no components at all.
 */
template <typename BaseSchema> using Pure = Unit<BaseSchema>;

} // namespace sch

namespace impl {

template <typename T, typename U> class unit_matching;

/**
 * A plain type list of UnitElements, used only to carry a pack around while
 * the factors are being reduced. It holds no members: the specialisation below
 * exists purely so that a non empty pack does not hit this assert.
 */
template <typename... T> class unit_redux_list {
	static_assert(sizeof...(T) == 0u, "Template type not supported");
};

template <typename T0, int64_t E0, int64_t S0, typename... TL, int64_t... EL,
		  int64_t... SL>
struct unit_redux_list<sch::UnitElement<T0, E0, S0>,
					   sch::UnitElement<TL, EL, SL>...> {};

template <typename T, typename U, typename V> class unit_matching_reduce {
public:
	static_assert(always_false<T, U, V>, "Template type not supported");
};

/**
 * Walks the remaining list looking for factors of the same base quantity and
 * scale as the one being accumulated, folding their exponents together.
 */
template <typename T, int64_t E, int64_t S, typename T0, int64_t E0, int64_t S0,
		  typename... TL, int64_t... EL, int64_t... SL, typename... TR,
		  int64_t... ER, int64_t... SR>
class unit_matching_reduce<
	sch::UnitElement<T, E, S>,
	unit_redux_list<sch::UnitElement<T0, E0, S0>,
					sch::UnitElement<TL, EL, SL>...>,
	unit_redux_list<sch::UnitElement<TR, ER, SR>...>> {
public:
	static constexpr bool is_same = std::is_same_v<T, T0> and (S == S0);

	using ReducedType =
		typename std::conditional<is_same, sch::UnitElement<T, E + E0, S>,
								  sch::UnitElement<T, E, S>>::type;
	using ReducedTypeList = typename std::conditional<
		is_same, unit_redux_list<sch::UnitElement<TR, ER, SR>...>,
		unit_redux_list<sch::UnitElement<TR, ER, SR>...,
						sch::UnitElement<T0, E0, S0>>>::type;

	using NextMatcher =
		unit_matching_reduce<ReducedType,
							 unit_redux_list<sch::UnitElement<TL, EL, SL>...>,
							 ReducedTypeList>;
	using Type = typename NextMatcher::Type;
	using TypeList = typename NextMatcher::TypeList;
	static constexpr int64_t Num = NextMatcher::Num;
};

/**
 * End of the walk: the accumulated factor on the left, everything that did not
 * match on the right.
 */
template <typename T, int64_t E, int64_t S, typename... TR, int64_t... ER,
		  int64_t... SR>
class unit_matching_reduce<sch::UnitElement<T, E, S>, unit_redux_list<>,
						   unit_redux_list<sch::UnitElement<TR, ER, SR>...>> {
public:
	using Type = sch::UnitElement<T, E, S>;
	using TypeList = unit_redux_list<sch::UnitElement<TR, ER, SR>...>;
	static constexpr int64_t Num = E;
};

template <typename LR, typename RR> class unit_matching {
	static_assert(always_false<LR, RR>, "Template type not supported");
};

template <typename... Ele>
class unit_matching<unit_redux_list<>, unit_redux_list<Ele...>> {
public:
	using TypeList = unit_redux_list<Ele...>;
};

template <typename Ele0, typename... EleL, typename... EleR>
class unit_matching<unit_redux_list<Ele0, EleL...>, unit_redux_list<EleR...>> {
public:
	using MatchReducer =
		unit_matching_reduce<Ele0, unit_redux_list<EleL...>, unit_redux_list<>>;

	using ReducedType = typename MatchReducer::Type;
	using ReducedTypeList = typename MatchReducer::TypeList;
	static constexpr int64_t Num = MatchReducer::Num;

	/**
	 * A factor whose exponents cancelled to zero is dropped rather than kept
	 * as a zero power, so that m/m really is dimensionless.
	 */
	using TypeRightList =
		typename std::conditional<Num == 0, unit_redux_list<EleR...>,
								  unit_redux_list<EleR..., ReducedType>>::type;
	using TypeList = typename unit_matching<ReducedTypeList, TypeRightList>::TypeList;
};

/**
 * Whether a schema is a Unit at all. Lets the arithmetic below tell a united
 * operand from a bare one.
 */
template <typename S> struct is_unit_schema : std::false_type {};

template <typename B, typename... E>
struct is_unit_schema<sch::Unit<B, E...>> : std::true_type {};

/**
 * Splits a Unit into its base schema and its component list, so the list can
 * be re-hosted on a different base.
 *
 * Needed because multiplying a unit vector by a unit scalar changes the base
 * (Vector * Scalar -> Vector) while the component lists still have to be
 * combined against each other. unit_multiplication requires both operands to
 * share a base, so both are rebased onto the result first.
 */
template <typename U> struct unit_parts;

template <typename B, typename... E> struct unit_parts<sch::Unit<B, E...>> {
	using Base = B;
	template <typename NewBase> using Rebase = sch::Unit<NewBase, E...>;
};

/**
 * A total order on unit factors, so that the same physical unit is always the
 * same C++ type.
 *
 * Without this the reduction keeps encounter order, and m * s^-1 produces a
 * different type from s^-1 * m -- the same quantity reached two ways then
 * fails to match, and the mismatch shows up as a baffling template error far
 * from its cause. Sorting by the base tag name gives a canonical form; the
 * scale breaks ties so that factors differing only in prefix still order
 * deterministically.
 */
template <typename A, typename B> struct unit_element_less;

template <typename TA, int64_t EA, int64_t SA, typename TB, int64_t EB, int64_t SB>
struct unit_element_less<sch::UnitElement<TA, EA, SA>, sch::UnitElement<TB, EB, SB>> {
	static constexpr bool value =
		(TA::name != TB::name) ? (TA::name < TB::name) : (SA < SB);
};

template <typename Ele, typename List> struct unit_prepend;

template <typename Ele, typename... L>
struct unit_prepend<Ele, unit_redux_list<L...>> {
	using Type = unit_redux_list<Ele, L...>;
};

template <typename Ele, typename List> struct unit_insert;

template <typename Ele> struct unit_insert<Ele, unit_redux_list<>> {
	using Type = unit_redux_list<Ele>;
};

template <typename Ele, typename Head, typename... Tail>
struct unit_insert<Ele, unit_redux_list<Head, Tail...>> {
	using Type = typename std::conditional<
		unit_element_less<Ele, Head>::value,
		unit_redux_list<Ele, Head, Tail...>,
		typename unit_prepend<
			Head,
			typename unit_insert<Ele, unit_redux_list<Tail...>>::Type>::Type>::type;
};

template <typename List> struct unit_sort;

template <> struct unit_sort<unit_redux_list<>> {
	using Type = unit_redux_list<>;
};

template <typename Head, typename... Tail>
struct unit_sort<unit_redux_list<Head, Tail...>> {
	using Type = typename unit_insert<
		Head, typename unit_sort<unit_redux_list<Tail...>>::Type>::Type;
};

template <typename BaseSchema, typename List> class unit_add_base {
	static_assert(always_false<BaseSchema, List>, "Template type not supported");
};

template <typename BaseSchema, typename... Ele>
class unit_add_base<BaseSchema, unit_redux_list<Ele...>> {
public:
	using Schema = sch::Unit<BaseSchema, Ele...>;
};

} // namespace impl

template <typename BaseSchema, typename... UE> struct unit_element_reduction {
	// Sorted after reducing, so the result is canonical regardless of the
	// order the factors arrived in.
	using TypeList = typename impl::unit_sort<
		typename impl::unit_matching<impl::unit_redux_list<UE...>,
									 impl::unit_redux_list<>>::TypeList>::Type;
	using Schema = typename impl::unit_add_base<BaseSchema, TypeList>::Schema;
};

/**
 * Labels a bare value with a unit.
 *
 * The deliberate way to cross from untyped to typed now that the wrapping
 * constructor is explicit. Being a named function, every such crossing is
 * greppable instead of happening silently at a call site.
 */
template <typename UnitSchema, typename S,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
constexpr data<UnitSchema, Encoding> as_unit(const data<S, Encoding> &value) {
	return data<UnitSchema, Encoding>{value};
}

template <typename U0, typename U1> struct unit_multiplication {
	static_assert(always_false<U0, U1>, "Template type not supported");
};

/**
 * The scale packs have to be deduced explicitly. Writing
 * `sch::UnitElement<UnitT, UnitE>` here would mean
 * `sch::UnitElement<UnitT, UnitE, 0>` after the default argument is
 * substituted, so the specialisation would only ever match zero scaled factors
 * and anything prefixed would fall through to the always_false primary.
 */
template <typename BaseSchema, typename... UnitT, int64_t... UnitE,
		  int64_t... UnitS, typename... UnitRhsT, int64_t... UnitRhsE,
		  int64_t... UnitRhsS>
struct unit_multiplication<
	sch::Unit<BaseSchema, sch::UnitElement<UnitT, UnitE, UnitS>...>,
	sch::Unit<BaseSchema, sch::UnitElement<UnitRhsT, UnitRhsE, UnitRhsS>...>> {
	using Schema = typename unit_element_reduction<
		BaseSchema, sch::UnitElement<UnitT, UnitE, UnitS>...,
		sch::UnitElement<UnitRhsT, UnitRhsE, UnitRhsS>...>::Schema;
};

template <typename U0, typename U1> struct unit_division {
	static_assert(always_false<U0, U1>, "Template type not supported");
};

/**
 * Division is multiplication by the negated exponents.
 *
 * The scale is deliberately NOT negated. Scale says which base unit is meant
 * -- km rather than m -- while the exponent says what power it is raised to.
 * So (km)^-1 is scale 3, exponent -1; negating the scale as well would quietly
 * turn it into (mm)^-1. The symmetry with the exponent is only apparent.
 */
template <typename BaseSchema, typename... UnitT, int64_t... UnitE,
		  int64_t... UnitS, typename... UnitRhsT, int64_t... UnitRhsE,
		  int64_t... UnitRhsS>
struct unit_division<
	sch::Unit<BaseSchema, sch::UnitElement<UnitT, UnitE, UnitS>...>,
	sch::Unit<BaseSchema, sch::UnitElement<UnitRhsT, UnitRhsE, UnitRhsS>...>> {
	using Schema = typename unit_element_reduction<
		BaseSchema, sch::UnitElement<UnitT, UnitE, UnitS>...,
		sch::UnitElement<UnitRhsT, -UnitRhsE, UnitRhsS>...>::Schema;
};

/**
 * A united value. It wraps the plain value and only allows arithmetic that the
 * unit algebra agrees with: addition needs identical units, while
 * multiplication and division derive the resulting unit.
 */
template <typename BaseSchema, typename... Elements>
class data<sch::Unit<BaseSchema, Elements...>, enc::Native> final {
public:
	using Schema = sch::Unit<BaseSchema, Elements...>;
	using MetaSchema = typename Schema::Meta;
	using BaseData = data<BaseSchema, enc::Native>;

private:
	BaseData dat_;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

/**
	 * Explicit: an already built value of the base type does not get to pick
	 * up a unit just because a unit was expected there. That is how an
	 * untyped sub step scalar silently became a time.
	 */
	explicit data(const BaseData &dat__) : dat_{dat__} {}

	explicit data(BaseData &&dat__) : dat_{std::move(dat__)} {}

	/**
	 * A bare literal is still allowed to become a united quantity at a
	 * construction site, because that is where the author is stating the
	 * unit. Only primitives, so this never competes with the base copy above
	 * for tensor valued units.
	 */
	template <typename V, typename B = BaseSchema>
		requires(sch::is_primitive<B>::value && std::is_arithmetic_v<V>)
	data(V value__)
		: dat_{static_cast<typename native_data_type<B>::type>(value__)} {}

	data<Schema, enc::Native> operator+(const data<Schema, enc::Native> &rhs) const {
		return data<Schema, enc::Native>{dat_ + rhs.handle()};
	}

	data<Schema, enc::Native> operator-(const data<Schema, enc::Native> &rhs) const {
		return data<Schema, enc::Native>{dat_ - rhs.handle()};
	}

	data<Schema, enc::Native> &operator+=(const data<Schema, enc::Native> &rhs) {
		dat_ += rhs.handle();
		return *this;
	}

	data<Schema, enc::Native> &operator-=(const data<Schema, enc::Native> &rhs) {
		dat_ -= rhs.handle();
		return *this;
	}

	bool operator==(const data<Schema, enc::Native> &rhs) const {
		return dat_ == rhs.handle();
	}

	bool operator!=(const data<Schema, enc::Native> &rhs) const {
		return !(dat_ == rhs.handle());
	}

	bool operator<(const data<Schema, enc::Native> &rhs) const {
		return dat_ < rhs.handle();
	}

	bool operator>(const data<Schema, enc::Native> &rhs) const {
		return dat_ > rhs.handle();
	}

/**
	 * The operands need not share a base schema: a length vector times a
	 * dimensionless scalar is a length vector. The base of the result is
	 * whatever the underlying arithmetic produces, and both component lists
	 * are rebased onto it before being combined.
	 */
	template <typename UnitRhs>
		requires impl::is_unit_schema<UnitRhs>::value
	auto operator*(const data<UnitRhs, enc::Native> &rhs) const {
		auto raw = dat_ * rhs.handle();
		using ResultBase = typename std::remove_cvref_t<decltype(raw)>::Schema;
		using ResultSchema = typename unit_multiplication<
			typename impl::unit_parts<Schema>::template Rebase<ResultBase>,
			typename impl::unit_parts<UnitRhs>::template Rebase<ResultBase>>::Schema;
		return data<ResultSchema, enc::Native>{raw};
	}

	template <typename UnitRhs>
		requires impl::is_unit_schema<UnitRhs>::value
	auto operator/(const data<UnitRhs, enc::Native> &rhs) const {
		auto raw = dat_ / rhs.handle();
		using ResultBase = typename std::remove_cvref_t<decltype(raw)>::Schema;
		using ResultSchema = typename unit_division<
			typename impl::unit_parts<Schema>::template Rebase<ResultBase>,
			typename impl::unit_parts<UnitRhs>::template Rebase<ResultBase>>::Schema;
		return data<ResultSchema, enc::Native>{raw};
	}

	/**
	 * The underlying value, with the unit stripped. Mostly for the
	 * multiplication and division operators above.
	 */
/**
	 * Element access forwards to the underlying tensor and returns the raw
	 * element.
	 *
	 * The unit belongs to the quantity as a whole, not to one component -- a
	 * position is metres in every component -- so there is nothing distinct to
	 * attach per element. The wrapper is also not stored per element, so a
	 * reference to a unit typed component could not be formed even if it were
	 * wanted. Mixing is still caught, just at the vector level.
	 */
	/**
	 * The rank is taken through a defaulted template parameter so that it is
	 * a substitution failure, not a hard error, when the base is not a tensor.
	 * The parameter type itself stays concrete, because a braced index like
	 * at({{0u}}) cannot be deduced against a template parameter.
	 */
	template <typename B = BaseSchema>
	auto &at(const index_data<B::Rank> &index) {
		return dat_.at(index);
	}

	template <typename B = BaseSchema>
	const auto &at(const index_data<B::Rank> &index) const {
		return dat_.at(index);
	}

	auto &flat_at(uint64_t i) { return dat_.flat_at(i); }

	const auto &flat_at(uint64_t i) const { return dat_.flat_at(i); }

	const BaseData &handle() const { return dat_; }

	BaseData &handle() { return dat_; }
};

} // namespace lbm
} // namespace kel
