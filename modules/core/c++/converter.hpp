#pragma once

#include "lbm_unit.hpp"
#include "descriptor.hpp"

namespace kel {
namespace lbm {

namespace sch {
}

/**
 * Helps converting from SI types to LBM types
 */
template<typename T>
class converter {
private:
	kel::lbm::data<typename kel::lbm::unit_division<sch::SiMeter<T>, sch::LbmMeter<T> >::Schema > meter_conv_; 
	kel::lbm::data<typename kel::lbm::unit_division<sch::SiSecond<T>, sch::LbmSecond<T> >::Schema > second_conv_; 
public:
	converter() = delete;
	explicit converter(
		kel::lbm::data<typename kel::lbm::unit_division<sch::SiMeter<T>, sch::LbmMeter<T> >::Schema > meter_conv__,
		kel::lbm::data<typename kel::lbm::unit_division<sch::SiSecond<T>, sch::LbmSecond<T> >::Schema > second_conv__
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
		return meter_conv_*kel::lbm::data<sch::LbmMeter<T>>{1.0};
	}

	auto delta_t() const {
		return second_conv_*kel::lbm::data<sch::LbmSecond<T>>{1.0};
	}

	auto delta_v() const {
		return (meter_conv_ / second_conv_) * kel::lbm::data<sch::LbmVelocity<T>>{1.0};
	}
	
	auto delta_a() const {
		return (meter_conv_ / (second_conv_*second_conv_)) * kel::lbm::data<sch::LbmAcceleration<T>>{1.0};
	}

	kel::lbm::data<sch::LbmMeter<T>> meter_si_to_lbm(const kel::lbm::data<sch::SiMeter<T>>& m_si) const {
		return m_si / meter_conv_;
	}
	
	kel::lbm::data<sch::SiMeter<T>> meter_lbm_to_si(const kel::lbm::data<sch::LbmMeter<T>>& m_lbm) const {
		return m_lbm * meter_conv_;
	}

	kel::lbm::data<sch::LbmSecond<T>> second_si_to_lbm(const kel::lbm::data<sch::SiSecond<T>>& s_si) const {
		return s_si / second_conv_;
	}
	
	kel::lbm::data<sch::LbmVelocity<T>> velocity_si_to_lbm(const kel::lbm::data<sch::SiVelocity<T>>& vel_si) const {
		return (vel_si / meter_conv_) * second_conv_;
	}
	
	kel::lbm::data<sch::LbmAcceleration<T>> acceleration_si_to_lbm(const kel::lbm::data<sch::SiAcceleration<T>>& acc_si) const {
		return (acc_si / meter_conv_) * (second_conv_ * second_conv_); 
	}

	kel::lbm::data<sch::LbmKinematicViscosity<T>> kinematic_viscosity_si_to_lbm (const kel::lbm::data<sch::SiKinematicViscosity<T>>& kin_si) const {
		return (kin_si / (meter_conv_ * meter_conv_)) * second_conv_;
	}

	template<typename Desc>
	kel::lbm::data<sch::Pure<T>> kinematic_viscosity_si_to_tau(const kel::lbm::data<sch::SiKinematicViscosity<T>>& kin_si) const {
		return kel::lbm::data<sch::Pure<T>>{kel::lbm::data<typename kel::lbm::unit_division<sch::Pure<T>, sch::LbmKinematicViscosity<T>>::Schema >{df_info<T,Desc>::inv_cs2} * kinematic_viscosity_si_to_lbm(kin_si) + kel::lbm::data<sch::Pure<T>>{0.5}};
	}

	template<typename Desc>
	kel::lbm::data<sch::Pure<T>> kinematic_viscosity_lbm_to_tau(const kel::lbm::data<sch::LbmKinematicViscosity<T>>& kin_lbm) const {
		return kel::lbm::data<sch::Pure<T>>{kel::lbm::data<typename kel::lbm::unit_division<sch::Pure<T>, sch::LbmKinematicViscosity<T>>::Schema >{df_info<T,Desc>::inv_cs2} * kin_lbm + kel::lbm::data<sch::Pure<T>>{0.5}};
	}
};
}
}
