#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common.hpp"
#include "string_literal.hpp"

namespace kel {
namespace lbm {

/**
 * This type is meant for grouping of template types
 */
template <class... T> struct tmpl_group {
	static constexpr string_literal name = "tmpl_group";
	static constexpr uint64_t size = sizeof...(T);
};

template <typename T, T... values> struct tmpl_value_group {
	static constexpr string_literal name = "tmpl_value_group";
	static constexpr uint64_t size = sizeof...(values);
};

/**
 * Concats two tmpl_group into one
 */
template <typename T, typename U> struct tmpl_concat;

template <typename... T, typename... U>
struct tmpl_concat<tmpl_group<T...>, tmpl_group<U...>> {
	using type = tmpl_group<T..., U...>;
};

template <typename T, T... L, T... R>
struct tmpl_concat<tmpl_value_group<T, L...>, tmpl_value_group<T, R...>> {
	using type = tmpl_value_group<T, L..., R...>;
};

/**
 * Returns the position of the parameter type in the parameter pack
 */
template <class T, class... TL> struct parameter_pack_index;

template <class T, class... TL> struct parameter_pack_index<T, T, TL...> {
	static constexpr size_t value = 0u;
};

template <class T, class TL0, class... TL>
struct parameter_pack_index<T, TL0, TL...> {
	static constexpr size_t value = 1u + parameter_pack_index<T, TL...>::value;
};

template <size_t N, class... T> struct parameter_pack_type {
	static_assert(always_false<T...>,
				  "Should've been caught by the specializations");
};

template <class TN, class... T> struct parameter_pack_type<0, TN, T...> {
	using type = TN;
};

template <size_t N, class TN, class... T>
struct parameter_pack_type<N, TN, T...> {
	static_assert(sizeof...(T) > 0, "Exhausted parameters");
	static_assert(N > 0, "Invalid number. Should've been caught");
	using type = typename parameter_pack_type<N - 1, T...>::type;
};

/*
 * Nightmare inducing compiler problems found here. Somehow non-type
 * string_literals cannot be resolved as non-type primitive template values can.
 * This is the workaround
 */
template <string_literal V, string_literal Key0, string_literal... Keys>
struct parameter_key_pack_index_helper {
	static constexpr size_t value =
		(V == Key0)
			? (0u)
			: (1u + parameter_key_pack_index_helper<V, Keys...>::value);
};

template <string_literal V, string_literal Key0>
struct parameter_key_pack_index_helper<V, Key0> {
	static constexpr size_t value = (V == Key0) ? (0u) : (1u);
};

template <string_literal V, string_literal... Keys>
struct parameter_key_pack_index {
	static constexpr size_t value =
		parameter_key_pack_index_helper<V, Keys...>::value;
	static_assert(value < sizeof...(Keys),
				  "Provided string_literal doesn't exist in searched list");
};

template <size_t i, size_t s, string_literal Key0, string_literal... Keys>
struct parameter_key_pack_type_helper {
	static constexpr string_literal literal =
		parameter_key_pack_type_helper<i, s + 1, Keys...>::literal;
};

template <size_t i, string_literal Key0, string_literal... Keys>
struct parameter_key_pack_type_helper<i, i, Key0, Keys...> {
	static constexpr string_literal literal = Key0;
};

template <size_t i, string_literal... Keys> struct parameter_key_pack_type {
	static constexpr string_literal literal =
		parameter_key_pack_type_helper<i, 0, Keys...>::literal;

	static_assert(i < sizeof...(Keys), "Provided index is too large for list");
};

template <std::size_t i, std::size_t s, typename T, T V0, T... VN>
struct parameter_pack_value_helper {
	static constexpr T value = parameter_pack_value_helper<i, s + 1, T, VN...>::value;
};

template <std::size_t i, typename T, T V0, T... VN>
struct parameter_pack_value_helper<i, i, T, V0, VN...> {
	static constexpr T value = V0;
};

template <std::size_t i, typename T, T... Values> struct parameter_pack_value {
	static constexpr T value =
		parameter_pack_value_helper<i, 0, T, Values...>::value;
	static_assert(i < sizeof...(Values),
				  "Provided index is too large for list");
};

template <uint64_t I, typename T> struct tmpl_group_pack_type;

template <uint64_t I, typename... T>
struct tmpl_group_pack_type<I, tmpl_group<T...>> {
	using type = typename parameter_pack_type<I, T...>::type;
};

/**
 * Compile time product of a value pack. Empty pack is the multiplicative
 * identity, which is what makes a rank zero tensor hold exactly one element.
 */
template <typename T, T... V> struct ct_multiply;

template <typename T> struct ct_multiply<T> {
	static constexpr T value = 1;
};

template <typename T, T V0, T... VN> struct ct_multiply<T, V0, VN...> {
	static constexpr T value = V0 * ct_multiply<T, VN...>::value;
};

namespace impl {
template <typename T> struct ct_convert_digits_table_helper {
	static constexpr std::array<T, 16> table = {'0', '1', '2', '3', '4', '5',
											   '6', '7', '8', '9', 'A', 'B',
											   'C', 'D', 'E', 'F'};

	template <uint64_t i> static constexpr T value() {
		static_assert(i < 16, "Only conversion up to hex is supported");
		return table[i];
	};
};

template <uint64_t Num, uint64_t Base, uint64_t... Digs>
struct ct_convert_digits_helper {
	static constexpr size_t size =
		ct_convert_digits_helper<Num / Base, Base, Num % Base, Digs...>::size;
	static constexpr std::array<uint64_t, size> value =
		ct_convert_digits_helper<Num / Base, Base, Num % Base, Digs...>::value;
	static constexpr string_literal literal =
		ct_convert_digits_helper<Num / Base, Base, Num % Base, Digs...>::literal;
};

template <uint64_t Base, uint64_t... Digs>
struct ct_convert_digits_helper<0, Base, Digs...> {
	static constexpr size_t size = sizeof...(Digs);
	static constexpr std::array<uint64_t, size> value = {Digs...};
	static constexpr string_literal literal = {
		{ct_convert_digits_table_helper<char>::value<Digs>()..., '\0'}};
};

template <uint64_t Base> struct ct_convert_digits_helper<0, Base> {
	static constexpr size_t size = 0;
	static constexpr std::array<uint64_t, 1> value = {0};
	static constexpr string_literal literal = "0"_sl;
};
} // namespace impl

template <uint64_t Num, uint64_t Base> struct ct_convert_to_digits {
	static_assert(Base <= 16, "Only conversion up to hex is supported");

	static constexpr size_t size = impl::ct_convert_digits_helper<Num, Base>::size;
	static constexpr std::array<uint64_t, size> value =
		impl::ct_convert_digits_helper<Num, Base>::value;
	static constexpr string_literal literal =
		impl::ct_convert_digits_helper<Num, Base>::literal;
};

/**
 * Decimal rendering of a compile time number, for schema stringification.
 */
template <uint64_t Num>
using ct_decimal_literal = ct_convert_to_digits<Num, 10u>;

} // namespace lbm
} // namespace kel
