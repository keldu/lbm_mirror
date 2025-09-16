#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct HLBM {};
}

/**
 * HLBM collision operator for LBM
 */
template<typename T, typename Descriptor>
class component<T, Descriptor, cmpt::HLBM> {
private:
public:
	component() = default;
};

}
}
