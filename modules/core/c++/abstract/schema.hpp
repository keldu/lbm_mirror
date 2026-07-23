#pragma once

#include "string_literal.hpp"
#include "templates.hpp"

namespace kel {
namespace lbm {
namespace sch {
struct Void {};

struct UnsignedInteger {};
struct SignedInteger {};
struct FloatingPoint {};

template<typename StorageT, typename InterfaceT>
struct MixedPrecision {
	using Meta = Void;
	using StorageType = StorageT;
	using InterfaceType = InterfaceT;
};

template<typename PrimType, uint64_t N>
struct Primitive {
	using Meta = Void;
	using StorageType = PrimType;
	using InterfaceType = PrimType;
	static constexpr uint64_t Bytes = N;
};

using UInt8 = Primitive<UnsignedInteger,1u>;
using UInt16 = Primitive<UnsignedInteger,2u>;
using UInt32 = Primitive<UnsignedInteger,4u>;
using UInt64 = Primitive<UnsignedInteger,8u>;

using Int8 = Primitive<SignedInteger,1u>;
using Int16 = Primitive<SignedInteger,2u>;
using Int32 = Primitive<SignedInteger,4u>;
using Int64 = Primitive<SignedInteger,8u>;

using Float8 = Primitive<FloatingPoint,1u>;
using Float16 = Primitive<FloatingPoint,2u>;
using Float32 = Primitive<FloatingPoint,4u>;
using Float64 = Primitive<FloatingPoint,8u>;

template<typename Sch, uint64_t StorageBytes, uint64_t InterfaceBytes>
struct MixedPrecision {
	using Meta = Void;
	using Inner = Sch;
};

template<typename T, uint64_t... Dims>
struct FixedArray {
	using Meta = Void;
	using Inner = T;
	static constexpr std::array<uint64_t,sizeof...(Dims)> Dimensions{Dims...};
};

template<typename T, uint64_t Dims>
struct Array {
	using Meta = FixedArray<UInt64,Dims>;
	using Inner = T;
	static constexpr std::array<uint64_t,1u> Dimensions{Dims};
};

template<typename... T>
struct Tuple {
};

template<typename Value, string_literal Key>
struct Member {};

template<typename... SL>
struct Struct {};

}

template<typename Sch>
struct schema {
	using Type = Sch;
};

}
}
