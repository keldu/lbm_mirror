#pragma once

#include <type_traits>

#include "data.hpp"
#include "schema.hpp"

namespace kel {
namespace lbm {
namespace sch {

/**
 * One factor of a unit: a base quantity raised to `Expo`, at scale `Scal`.
 *
 * Scaling distinguishes units of the same quantity that are not
 * interchangeable, e.g. lattice metres from SI metres, so that mixing them is
 * a compile time error rather than a silent numeric mistake.
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

template <typename... T> class unit_redux_list {
	static_assert(sizeof...(T) == 0u, "Template type not supported");

	using Type = unit_redux_list<>;
};

template <typename T0, int64_t E0, int64_t S0, typename... TL, int64_t... EL,
		  int64_t... SL>
struct unit_redux_list<sch::UnitElement<T0, E0, S0>,
					   sch::UnitElement<TL, EL, SL>...> {
	using Type = typename unit_matching<
		unit_redux_list<sch::UnitElement<T0, E0, S0>,
						sch::UnitElement<TL, EL, SL>...>,
		unit_redux_list<>>::TypeList;
};

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
	static constexpr int64_t Scale = S;
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
	using TypeList =
		typename impl::unit_matching<impl::unit_redux_list<UE...>,
									 impl::unit_redux_list<>>::TypeList;
	using Schema = typename impl::unit_add_base<BaseSchema, TypeList>::Schema;
};

template <typename U0, typename U1> struct unit_multiplication {
	static_assert(always_false<U0, U1>, "Template type not supported");
};

template <typename BaseSchema, typename... UnitT, int64_t... UnitE,
		  typename... UnitRhsT, int64_t... UnitRhsE>
struct unit_multiplication<
	sch::Unit<BaseSchema, sch::UnitElement<UnitT, UnitE>...>,
	sch::Unit<BaseSchema, sch::UnitElement<UnitRhsT, UnitRhsE>...>> {
	using Schema = typename unit_element_reduction<
		BaseSchema, sch::UnitElement<UnitT, UnitE>...,
		sch::UnitElement<UnitRhsT, UnitRhsE>...>::Schema;
};

template <typename U0, typename U1> struct unit_division {
	static_assert(always_false<U0, U1>, "Template type not supported");
};

/**
 * Division is multiplication by the negated exponents.
 */
template <typename BaseSchema, typename... UnitT, int64_t... UnitE,
		  typename... UnitRhsT, int64_t... UnitRhsE>
struct unit_division<
	sch::Unit<BaseSchema, sch::UnitElement<UnitT, UnitE>...>,
	sch::Unit<BaseSchema, sch::UnitElement<UnitRhsT, UnitRhsE>...>> {
	using Schema = typename unit_element_reduction<
		BaseSchema, sch::UnitElement<UnitT, UnitE>...,
		sch::UnitElement<UnitRhsT, -UnitRhsE>...>::Schema;
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

	data(const BaseData &dat__) : dat_{dat__} {}

	data(BaseData &&dat__) : dat_{std::move(dat__)} {}

	data<Schema, enc::Native> operator+(const data<Schema, enc::Native> &rhs) const {
		return {dat_ + rhs.handle()};
	}

	data<Schema, enc::Native> operator-(const data<Schema, enc::Native> &rhs) const {
		return {dat_ - rhs.handle()};
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

	template <typename UnitRhs>
	data<typename unit_multiplication<Schema, UnitRhs>::Schema, enc::Native>
	operator*(const data<UnitRhs, enc::Native> &rhs) const {
		return {dat_ * rhs.handle()};
	}

	template <typename UnitRhs>
	data<typename unit_division<Schema, UnitRhs>::Schema, enc::Native>
	operator/(const data<UnitRhs, enc::Native> &rhs) const {
		return {dat_ / rhs.handle()};
	}

	/**
	 * The underlying value, with the unit stripped. Mostly for the
	 * multiplication and division operators above.
	 */
	const BaseData &handle() const { return dat_; }

	BaseData &handle() { return dat_; }
};

} // namespace lbm
} // namespace kel
