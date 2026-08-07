#pragma once

#include "../common.hpp"

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<uint64_t DV, uint64_t QV>
struct Descriptor {
	static constexpr uint64_t D = DV;
	static constexpr uint64_t Q = QV;
};

}

template<typename T, typename Desc>
class df_info{};

}
}
