#pragma once

#include <kel/unit/unit.h>

#include <string_view>

namespace kel {
namespace lbm_type {
struct meter {
	static constexpr std::string_view name = "meter_l";
	static constexpr std::string_view short_name = "m_l";
};

struct second {
	static constexpr std::string_view name = "second_l";
	static constexpr std::string_view short_name = "s_l";
};
}

template<typename S>
using lbm_meter = unit<S, unit_component<lbm_type::meter, 1>>;

template<typename S>
using lbm_second = unit<S, unit_component<lbm_type::second, 1>>;

template<typename S>
using lbm_velocity = unit<S, unit_component<lbm_type::meter, 1>, unit_component<lbm_type::second, -1>>;

template<typename S>
using lbm_acceleration = unit<S, unit_component<lbm_type::meter, 1>, unit_component<lbm_type::second, -2>>;
}
