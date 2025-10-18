#pragma once

#include "lbm_unit.hpp"
#include "descriptor.hpp"

namespace kel {
namespace lbm {

namespace sch {
using namespace saw::schema;
}

/**
 * Helps converting from SI types to LBM types
 */
template<typename T>
class converter {
private:
	saw::data<typename saw::unit_division<sch::SiMeter<T>, sch::LbmMeter<T> >::Schema > meter_conv_; 
	saw::data<typename saw::unit_division<sch::SiSecond<T>, sch::LbmSecond<T> >::Schema > second_conv_; 
public:
	converter() = delete;
	converter(
		saw::data<typename saw::unit_division<sch::SiMeter<T>, sch::LbmMeter<T> >::Schema > meter_conv__,
		saw::data<typename saw::unit_division<sch::SiSecond<T>, sch::LbmSecond<T> >::Schema > second_conv__
	):
		meter_conv_{meter_conv__},
		second_conv_{second_conv__}
	{}

	/**
	 * Get the conversion parameter with the conversion type
	 */
	auto conversion_x() const {
		return meter_conv_;
	}

	/**
	 * Get the conversion parameter with the conversion type
	 */
	auto conversion_t() const {
		return second_conv_;
	}

	auto delta_x() const {
		return meter_conv_*saw::data<sch::LbmMeter<T>>{1.0};
	}

	auto delta_t() const {
		return second_conv_*saw::data<sch::LbmSecond<T>>{1.0};
	}

	saw::data<sch::LbmMeter<T>> meter_si_to_lbm(const saw::data<sch::SiMeter<T>>& m_si) const {
		return m_si / meter_conv_;
	}
	
	saw::data<sch::LbmSecond<T>> second_si_to_lbm(const saw::data<sch::SiSecond<T>>& s_si) const {
		return s_si / second_conv_;
	}
	
	saw::data<sch::LbmVelocity<T>> velocity_si_to_lbm(const saw::data<sch::SiVelocity<T>>& vel_si) const {
		return vel_si * second_conv_ / meter_conv_;
	}
	
	saw::data<sch::LbmAcceleration<T>> acceleration_si_to_lbm(const saw::data<sch::SiAcceleration<T>>& acc_si) const {
		return acc_si * (second_conv_ * second_conv_) / meter_conv_;
	}

	saw::data<sch::LbmKinematicViscosity<T>> kinematic_viscosity_si_to_lbm (const saw::data<sch::SiKinematicViscosity<T>>& kin_si) const {
		return (kin_si / (meter_conv_ * meter_conv_)) * second_conv_;
	}

	template<typename Desc>
	saw::data<sch::Pure<T>> kinematic_viscosity_si_to_tau(const saw::data<sch::SiKinematicViscosity<T>>& kin_si) const {
		return saw::data<sch::Pure<T>>{saw::data<typename saw::unit_division<sch::Pure<T>, sch::LbmKinematicViscosity<T>>::Schema >{df_info<T,Desc>::inv_cs2} * kinematic_viscosity_si_to_lbm(kin_si) + saw::data<sch::Pure<T>>{0.5}};
	}
};
}
}
