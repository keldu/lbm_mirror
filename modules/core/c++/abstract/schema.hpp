#pragma once

#include "string_literal.hpp"
#include "templates.hpp"

namespace kel {
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
	static constexpr std::array<uint64_t,sizeof...(Dims)> Dimensions{Dims};
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
