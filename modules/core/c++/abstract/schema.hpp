#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "string_literal.hpp"
#include "templates.hpp"

namespace kel {
namespace lbm {
namespace sch {

/**
 * Every schema in here carries its own metadata schema as a nested `Meta`
 * typedef, rather than exposing it through a separate trait. `Meta` is the
 * schema of the runtime information a value needs before it can be addressed
 * at all -- the extents of a dynamically sized Array, the length of a String.
 * Schemas whose shape is fully known at compile time report `Meta = Void`.
 */

struct Void {
	using Meta = Void;
	static constexpr string_literal name = "Void";
};

struct Bool {
	using Meta = Void;
	static constexpr string_literal name = "Bool";
};

/**
 * Primitive kinds. These are tags describing how the bytes of a Primitive are
 * to be interpreted; they are not schemas in their own right.
 */
struct UnsignedInteger {
	static constexpr string_literal name = "UnsignedInteger";
};
struct SignedInteger {
	static constexpr string_literal name = "SignedInteger";
};
struct FloatingPoint {
	static constexpr string_literal name = "FloatingPoint";
};

template <typename T> struct is_primitive_kind : std::false_type {};
template <> struct is_primitive_kind<UnsignedInteger> : std::true_type {};
template <> struct is_primitive_kind<SignedInteger> : std::true_type {};
template <> struct is_primitive_kind<FloatingPoint> : std::true_type {};

/**
 * A primitive of kind `PrimType` occupying `N` bytes.
 *
 * `StorageType` and `InterfaceType` are both schemas, and for a plain
 * Primitive they are the type itself -- it is stored exactly as it is read.
 * MixedPrecision is the case where the two come apart. `Kind` names the
 * interpretation tag and `Bytes` the width.
 */
template <typename PrimType, uint64_t N> struct Primitive {
	static_assert(is_primitive_kind<PrimType>::value,
				  "Primitive needs one of UnsignedInteger, SignedInteger or "
				  "FloatingPoint as its kind");
	static_assert(((std::is_same_v<PrimType, SignedInteger> ||
					std::is_same_v<PrimType, UnsignedInteger>) &&
				   (N == 1u || N == 2u || N == 4u || N == 8u)) ||
					  (std::is_same_v<PrimType, FloatingPoint> &&
					   (N == 4u || N == 8u)),
				  "Primitive Type is not supported");

	using Meta = Void;
	using Kind = PrimType;
	using StorageType = Primitive<PrimType, N>;
	using InterfaceType = Primitive<PrimType, N>;
	static constexpr uint64_t Bytes = N;
	static constexpr string_literal name = "Primitive";
};

template <typename T> struct is_primitive : std::false_type {};
template <typename K, uint64_t N>
struct is_primitive<Primitive<K, N>> : std::true_type {};

using UInt8 = Primitive<UnsignedInteger, 1u>;
using UInt16 = Primitive<UnsignedInteger, 2u>;
using UInt32 = Primitive<UnsignedInteger, 4u>;
using UInt64 = Primitive<UnsignedInteger, 8u>;

using Int8 = Primitive<SignedInteger, 1u>;
using Int16 = Primitive<SignedInteger, 2u>;
using Int32 = Primitive<SignedInteger, 4u>;
using Int64 = Primitive<SignedInteger, 8u>;

using Float32 = Primitive<FloatingPoint, 4u>;
using Float64 = Primitive<FloatingPoint, 8u>;

/**
 * A value stored as `StorageT` but read and written as `InterfaceT`.
 *
 * Note the argument order: storage first, interface second. forstio's
 * schema::MixedPrecision took them the other way round, so any call site
 * carried over from there needs its arguments swapped.
 */
template <typename StorageT, typename InterfaceT> struct MixedPrecision {
	static_assert(is_primitive<StorageT>::value,
				  "StorageType needs to be a Primitive");
	static_assert(is_primitive<InterfaceT>::value,
				  "InterfaceType needs to be a Primitive");

	using Meta = Void;
	using StorageType = StorageT;
	using InterfaceType = InterfaceT;
	static constexpr string_literal name = "MixedPrecision";
};

/**
 * A variable length string. Its length is the metadata.
 */
struct String {
	using Meta = UInt64;
	static constexpr string_literal name = "String";
};

/**
 * Named member of a Struct or a Union.
 */
template <typename Value, string_literal Key_> struct Member {
	using ValueType = Value;
	static constexpr string_literal KeyLiteral = Key_;
	static constexpr string_literal Key = Key_;
	static constexpr string_literal name = "Member";
};

/**
 * Array whose extents are fixed at compile time, so there is nothing to carry
 * at runtime and `Meta` is Void.
 */
template <typename T, uint64_t... Dims> struct FixedArray {
	using Meta = Void;
	using Inner = T;
	/// Spelling carried over from forstio, kept so existing call sites read.
	using ValueType = T;
	static constexpr uint64_t Rank = sizeof...(Dims);
	static constexpr std::array<uint64_t, sizeof...(Dims)> Dimensions{Dims...};
	static constexpr uint64_t FlatSize = ct_multiply<uint64_t, Dims...>::value;
	static constexpr string_literal name = "FixedArray";
};

/**
 * Array of rank `Dims` whose extents are only known at runtime, so the extents
 * themselves are the metadata.
 */
template <typename T, uint64_t Dims = 1u> struct Array {
	using Meta = FixedArray<UInt64, Dims>;
	using Inner = T;
	using ValueType = T;
	static constexpr uint64_t Rank = Dims;
	static constexpr string_literal name = "Array";
};

/**
 * Heterogeneous fixed sequence. Its metadata is the tuple of its members'
 * metadata, so that a Tuple of dynamically sized members can still be built.
 */
template <typename... T> struct Tuple {
	using Meta = Tuple<typename T::Meta...>;
	static constexpr uint64_t Size = sizeof...(T);
	static constexpr string_literal name = "Tuple";
};

template <typename... T> struct Struct {
	static_assert(always_false<T...>,
				  "Struct only accepts Member<Schema,Key> parameters");
};

template <typename... Schema, string_literal... Key>
struct Struct<Member<Schema, Key>...> {
	using Meta = Struct<Member<typename Schema::Meta, Key>...>;
	static constexpr uint64_t Size = sizeof...(Schema);
	static constexpr string_literal name = "Struct";
};

template <typename... T> struct Union {
	static_assert(always_false<T...>,
				  "Union only accepts Member<Schema,Key> parameters");
};

template <typename... Schema, string_literal... Key>
struct Union<Member<Schema, Key>...> {
	using Meta = Union<Member<typename Schema::Meta, Key>...>;
	static constexpr uint64_t Size = sizeof...(Schema);
	static constexpr string_literal name = "Union";
};

template <typename Key, typename Value> struct Map {
	using Meta = Void;
	using KeyType = Key;
	using ValueType = Value;
	static constexpr string_literal name = "Map";
};

/**
 * The schema counterpart of ref<T>.
 */
template <typename Schema> struct Ref {
	using Meta = Void;
	using ValueType = Schema;
	static constexpr string_literal name = "Ref";
};

/**
 * The schema counterpart of ptr<T>.
 *
 * Its advantage is that data<Ptr<UInt64>> keeps the interface of
 * data<UInt64> -- arithmetic still reads the same -- while the storage is
 * borrowed rather than owned. This is what lets an Array<Float64> be handed
 * out as a double* without losing the data accessor on top of it.
 */
template <typename Schema> struct Ptr {
	using Meta = Void;
	using ValueType = Schema;
	static constexpr string_literal name = "Ptr";
};

/**
 * Separator between encodings that are nested in one another, e.g. a base64
 * blob transported inside JSON.
 */
template <typename Schema, typename Enc> struct Wrapper {
	using Meta = typename Schema::Meta;
	using ValueType = Schema;
	using Encoding = Enc;
	static constexpr string_literal name = "Wrapper";
};

/**
 * Tensor of rank sizeof...(D) over `Inner`.
 *
 * The extents are compile time constants, but the metadata is still the extent
 * list: consumers index a tensor through a rank sized index, and that index
 * schema is what `Meta` names.
 */
template <typename Inner, uint64_t... D> struct Tensor {
	using Meta = FixedArray<UInt64, sizeof...(D)>;
	using Inner_ = Inner;
	using ValueType = Inner;
	static constexpr uint64_t Rank = sizeof...(D);
	static constexpr std::array<uint64_t, Rank> Dimension{D...};
	static constexpr uint64_t FlatSize = ct_multiply<uint64_t, D...>::value;
	static constexpr string_literal name = "Tensor";
};

template <typename Inner> using Scalar = Tensor<Inner>;

template <typename Inner, uint64_t D> using Vector = Tensor<Inner, D>;

template <typename Inner, uint64_t D1, uint64_t D2>
using Matrix = Tensor<Inner, D1, D2>;

} // namespace sch

/**
 * Convenience accessor, so `Meta` can be spelled as a trait where a nested
 * typedef would be awkward to reach.
 */
template <typename Schema> struct meta_schema {
	using Schema_ = Schema;
	using MetaSchema = typename Schema::Meta;
};

template <typename Sch> struct schema { using Type = Sch; };

/**
 * Index into the members of a structural schema.
 */
template <uint64_t I, typename Structural> struct schema_type_at_index;

template <uint64_t I, typename... Members>
struct schema_type_at_index<I, sch::Struct<Members...>> {
	using Type = typename parameter_pack_type<I, Members...>::type;
};

template <uint64_t I, typename... Members>
struct schema_type_at_index<I, sch::Union<Members...>> {
	using Type = typename parameter_pack_type<I, Members...>::type;
};

template <uint64_t I, typename... T>
struct schema_type_at_index<I, sch::Tuple<T...>> {
	using Type = typename parameter_pack_type<I, T...>::type;
};

} // namespace lbm
} // namespace kel
