#pragma once

#include "descriptor.hpp"

namespace kel {
namespace lbm {

namespace cmpt {
template<saw::string_literal Name, uint64_t Dist, bool Read, bool Write>
class access {
public:
	static constexpr saw::string_literal name = Name;
	static constexpr uint64_t distance = Dist;
	static constexpr bool read = Read;
	static constexpr bool write = Write;
};

template<typename... Acc>
class access_tuple {
public:
};
}

/**
 * Compponent class which forms the basis of the 
 */
template<typename T>
class component {};
}
}
