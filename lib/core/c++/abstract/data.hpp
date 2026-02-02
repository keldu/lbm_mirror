#pragma once

#include "templates.hpp"

namespace kel {
namespace sch {
struct UnsignedInteger {};
struct SignedInteger {};
struct FloatingPoint {};

template<typename StorageT, typename InterfaceT>
struct MixedPrecision {
	using StorageType = StorageT;
	using InterfaceType = InterfaceT;
};

template<typename PrimType, uint64_t N>
struct Primitive {
	using PrimitiveType = PrimType;
	static constexpr uint64_t Bytes = N;
};

template<typename T, uint64_t... Dims>
struct Array {
	using InnerType = T;
	static constexpr std::array<uint64_t,sizeof...(Dims)> Dimensions{Dims...};
};


}
}
