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

namesapce sch {

template<typename S>
using SiMeter = schema::Unit<S, schema::UnitElement<si_type::meter, 1>>;

template<typename S>
using LbmMeter = schema::Unit<S, schema::UnitElement<lbm_type::meter, 1>>;

template<typename S>
using SiSecond = schema::Unit<S, schema::UnitElement<si_type::meter, 1>>;

template<typename S>
using LbmSecond = schema::Unit<S, schema::UnitElement<lbm_type::second, 1>>;

template<typename S>
using SiVelocity = schema::Unit<S, schema::UnitElement<si_type::meter, 1>, schema::UnitElement<si_type::second, -1>>;

template<typename S>
using LbmVelocity = schema::Unit<S, schema::UnitElement<lbm_type::meter, 1>, schema::UnitElement<lbm_type::second, -1>>;

template<typename S>
using SiAcceleration = schema::Unit<S, schema::UnitElement<si_type::meter, 1>, schema::UnitElement<si_type::second, -2>>;

template<typename S>
using LbmAcceleration = schema::Unit<S, schema::UnitElement<lbm_type::meter, 1>, schema::UnitElement<lbm_type::second, -2>>;

}
}
}
