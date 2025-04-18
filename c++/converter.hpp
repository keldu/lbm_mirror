#pragma once

#include "lbm_unit.hpp"

namespace kel {
namespace lbm {

template<typename T>
class converter {
private:
	saw::data<saw::unit_division<T, LbmMeter<T>, SiMeter<T> >> meter_conv_; 
	saw::data<saw::unit_division<T, LbmSecond<T>, SiSecond<T> >> second_conv_; 
public:
	converter() = delete;
	converter(
		saw::data<saw::unit_division<T, LbmMeter<T>, SiMeter<T> >> meter_conv__,
		saw::data<saw::unit_division<T, LbmSecond<T>, SiSecond<T> >> second_conv__
	):
		meter_conv_{meter_conv__},
		second_conv_{second_conv__}
	{}

	saw::data<sch::LbmMeter<T>> meter_si_to_lbm(const saw::data<sch::SiMeter<T>>& m_si){
		return m_si * meter_conv_;
	}
	
	saw::data<sch::LbmSecond<T>> second_si_to_lbm(const saw::data<sch::SiSecond<T>>& s_si){
		return s_si * second_conv_;
	}
	
	saw::data<sch::LbmVelocity<T>> velocity_si_to_lbm(const saw::data<sch::SiVelocity<T>>& vel_si){
		return vel_si * meter_conv_ / second_conv_;
	}
	
	saw::data<sch::LbmAcceleration<T>> acceleration_si_to_lbm(const saw::data<sch::SiAcceleration<T>>& acc_si){
		return acc_si * meter_conv_ / (second_conv_ * second_conv_);
	}

};
}
}
