#pragma once

#include "schema.hpp"

namespace kel {
namespace lbm {

/**
 * Compile time predicate: does this structural schema already carry `Key`?
 */
template <string_literal Key, typename Schema> struct schema_has_key;

template <string_literal Key, typename... T, string_literal... Keys>
struct schema_has_key<Key, sch::Struct<sch::Member<T, Keys>...>> {
	static constexpr bool value = ((Key == Keys) || ...);
};

template <string_literal Key, typename... T, string_literal... Keys>
struct schema_has_key<Key, sch::Union<sch::Member<T, Keys>...>> {
	static constexpr bool value = ((Key == Keys) || ...);
};

/**
 * Compile time predicate: does this structural schema already carry exactly
 * this member, key and type both?
 */
template <typename Member, typename Schema> struct schema_has_member;

template <typename MT, string_literal MK, typename... T, string_literal... Keys>
struct schema_has_member<sch::Member<MT, MK>,
						 sch::Struct<sch::Member<T, Keys>...>> {
	static constexpr bool value =
		((MK == Keys && std::is_same_v<MT, T>) || ...);
};

template <typename MT, string_literal MK, typename... T, string_literal... Keys>
struct schema_has_member<sch::Member<MT, MK>,
						 sch::Union<sch::Member<T, Keys>...>> {
	static constexpr bool value =
		((MK == Keys && std::is_same_v<MT, T>) || ...);
};

/**
 * Builds a structural schema a member at a time. Every add() yields a new
 * factory over a wider schema, so the schema is assembled purely in the type
 * system.
 */
template <typename T> struct schema_factory {
	using Schema = T;
	static_assert(always_false<T>, "Not supported");
};

template <typename... T, string_literal... Keys>
struct schema_factory<sch::Struct<sch::Member<T, Keys>...>> {
	using Schema = sch::Struct<sch::Member<T, Keys>...>;

	template <string_literal KA> static constexpr bool has_key() noexcept {
		return schema_has_key<KA, Schema>::value;
	}

	template <typename TA, string_literal KA>
	static constexpr bool has_member() noexcept {
		return schema_has_member<sch::Member<TA, KA>, Schema>::value;
	}

	/**
	 * Adds the member unless it is already there. Re-adding the very same
	 * member is a no-op; re-using a key for a different type is an error,
	 * because that would silently change what the existing key means.
	 */
	template <typename TA, string_literal KA>
	constexpr auto add_maybe() const noexcept {
		if constexpr (!has_key<KA>()) {
			return schema_factory<
				sch::Struct<sch::Member<T, Keys>..., sch::Member<TA, KA>>>{};
		} else if constexpr (has_member<TA, KA>()) {
			return schema_factory<sch::Struct<sch::Member<T, Keys>...>>{};
		} else {
			static_assert(
				always_false<TA>,
				"Struct already has the provided Key, but a mismatching Type");
		}
	}

	template <typename TA, string_literal KA>
	constexpr schema_factory<
		sch::Struct<sch::Member<T, Keys>..., sch::Member<TA, KA>>>
	add() const noexcept {
		static_assert(!has_key<KA>(),
					  "Can't add member. Name is already in use.");
		return {};
	}
};

template <typename... T, string_literal... Keys>
struct schema_factory<sch::Union<sch::Member<T, Keys>...>> {
	using Schema = sch::Union<sch::Member<T, Keys>...>;

	template <string_literal KA> static constexpr bool has_key() noexcept {
		return schema_has_key<KA, Schema>::value;
	}

	template <typename TA, string_literal KA>
	constexpr schema_factory<
		sch::Union<sch::Member<T, Keys>..., sch::Member<TA, KA>>>
	add() const noexcept {
		static_assert(!has_key<KA>(),
					  "Can't add member. Name is already in use.");
		return {};
	}
};

template <typename... T> struct schema_factory<sch::Tuple<T...>> {
	using Schema = sch::Tuple<T...>;

	template <typename TA>
	constexpr schema_factory<sch::Tuple<T..., TA>> add() const noexcept {
		return {};
	}
};

/**
 * Creates the base schema to build on: an empty struct, tuple or union.
 */
template <typename T> constexpr schema_factory<T> build_schema() noexcept {
	return {};
}

} // namespace lbm
} // namespace kel
