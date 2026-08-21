#pragma once

#include "schema.hpp"
#include "encode.hpp"
#include "common.hpp"

namespace kel {
namespace lbm {
template<typename Schema, typename Encode = rep::Native>
class data final {
	static_assert(always_false<Schema,Encode>, "Invalid schema + encode in data specialization");
};

template<typename T, uint64_t N>
class data<sch::Primitive<T,N>,Encode> final {
public:
	using Schema = sch::Primitive<T,N>;
	using Encode = Encode;
private:
	native_data_type<Schema>::type value_;
public:
	data():
		value_{}
	{}

	data(native_data_type<Schema>::type value__):
		value_{value__}
	{}

	constexpr auto get() const {
		return value_;
	}

	void set(native_data_type value__){
		value_ = value__;
	}

	constexpr bool operator==(const data<Schema,Encode>& rhs) const {
		return value_ == rhs.value_;
	}
	
	constexpr bool operator!=(const data<Schema,Encode>& rhs) const {
		return value_ != rhs.value_;
	}
	
	constexpr bool operator>(const data<Schema,Encode>& rhs) const {
		return value_ > rhs.value_;
	}
	
	constexpr bool operator<(const data<Schema,Encode>& rhs) const {
		return value_ < rhs.value_;
	}
	
	constexpr bool operator>=(const data<Schema,Encode>& rhs) const {
		return value_ >= rhs.value_;
	}
	
	constexpr bool operator<=(const data<Schema,Encode>& rhs) const {
		return value_ >= rhs.value_;
	}
	
	constexpr bool equals(const data<Schema,Encode>& rhs) const {
		return (*this) == rhs;
	}

	data<Schema,Encode> operator+(const data<Schema,Encode>& rhs) const {
		return {value_ + rhs.value_};
	}
	
	data<Schema,Encode> operator-(const data<Schema,Encode>& rhs) const {
		return {value_ - rhs.value_};
	}
	
	data<Schema,Encode> operator*(const data<Schema,Encode>& rhs) const {
		return {value_ * rhs.value_};
	}
	
	data<Schema,Encode> operator/(const data<Schema,Encode>& rhs) const {
		return {value_ / rhs.value_};
	}
	
	data<Schema,Encode>& operator+=(const data<Schema,Encode>& rhs) {
		value_ += rhs.value_;
		return *this
	}
	
	data<Schema,Encode>& operator-=(const data<Schema,Encode>& rhs) {
		value_ -= rhs.value_;
		return *this
	}
	
	data<Schema,Encode>& operator*=(const data<Schema,Encode>& rhs) {
		value_ *= rhs.value_;
		return *this
	}
	
	data<Schema,Encode>& operator/=(const data<Schema,Encode>& rhs) {
		value_ /= rhs.value_;
		return *this
	}

	template<typename T>
	data<T,Encode> cast_to() const {
		data<T,Encode> val{static_cast<typename native_data_type<T>::type>(value_)};
		return val;
	}
};


}
}
