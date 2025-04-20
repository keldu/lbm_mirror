#pragma once

#include <forstio/codec/unit/unit.hpp>

#include <string_view>

namespace kel {
namespace lbm {
namespace lbm_type {
struct meter {
	static constexpr std::string_view name = "meter_lbm";
	static constexpr std::string_view short_name = "m_lbm";
};

struct second {
	static constexpr std::string_view name = "second_lbm";
	static constexpr std::string_view short_name = "s_lbm";
};
}

namespace si_type {
struct meter {
	static constexpr std::string_view name = "meter_si";
	static constexpr std::string_view short_name = "m_si";
};

struct second {
	static constexpr std::string_view name = "second_si";
	static constexpr std::string_view short_name = "s_si";
};
}

namespace sch {
using namespace saw::schema;
template<typename S>
using SiMeter = Unit<S, UnitElement<si_type::meter, 1>>;

template<typename S>
using LbmMeter = Unit<S, UnitElement<lbm_type::meter, 1>>;

template<typename S>
using SiSecond = Unit<S, UnitElement<si_type::meter, 1>>;

template<typename S>
using LbmSecond = Unit<S, UnitElement<lbm_type::second, 1>>;

template<typename S>
using SiVelocity = Unit<S, UnitElement<si_type::meter, 1>, UnitElement<si_type::second, -1>>;

template<typename S>
using LbmVelocity = Unit<S, UnitElement<lbm_type::meter, 1>, UnitElement<lbm_type::second, -1>>;

template<typename S>
using SiAcceleration = Unit<S, UnitElement<si_type::meter, 1>, UnitElement<si_type::second, -2>>;

template<typename S>
using LbmAcceleration = Unit<S, UnitElement<lbm_type::meter, 1>, UnitElement<lbm_type::second, -2>>;

template<typename S>
using SiKinematicViscosity = Unit<S, UnitElement<si_type::meter, 2>, UnitElement<si_type::second, -1>>;

template<typename S>
using LbmKinematicViscosity = Unit<S, UnitElement<lbm_type::meter, 2>, UnitElement<lbm_type::second, -1>>;

}
}
}
