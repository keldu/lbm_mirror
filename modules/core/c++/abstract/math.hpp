#pragma once

#include <cmath>

#include "data.hpp"
#include "data_math.hpp"

namespace kel {
namespace lbm {
namespace math {

/**
 * Builds a scalar from its native value. Shorthand for the two step
 * data<T>{value} -> data<Scalar<T>>{...} conversion, which the language won't
 * do implicitly.
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
constexpr data<sch::Scalar<T>, Encoding>
make_scalar(typename native_data_type<T>::type value) {
	return data<sch::Scalar<T>, Encoding>{data<T, Encoding>{value}};
}

/**
 * Builds a vector from its native component values. The dimension is deduced
 * from the argument count.
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING,
		  typename... Args>
constexpr data<sch::Vector<T, sizeof...(Args)>, Encoding>
make_vector(Args... args) {
	return data<sch::Vector<T, sizeof...(Args)>, Encoding>{
		data<sch::FixedArray<T, sizeof...(Args)>, Encoding>{
			std::array<data<T, Encoding>, sizeof...(Args)>{data<T, Encoding>{
				static_cast<typename native_data_type<T>::type>(args)}...}}};
}

template <typename T, uint64_t D,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding> dot(const data<sch::Vector<T, D>, Encoding> &left,
								   const data<sch::Vector<T, D>, Encoding> &right) {
	data<sch::Scalar<T>, Encoding> val;
	auto &inner = val.flat_at(0u);
	for (uint64_t i = 0u; i < D; ++i) {
		inner = inner + left.flat_at(i) * right.flat_at(i);
	}

	return val;
}

template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding> sqrt(const data<sch::Scalar<T>, Encoding> &inp) {
	data<sch::Scalar<T>, Encoding> out;
	out.flat_at(0u).set(std::sqrt(inp.flat_at(0u).get()));
	return out;
}

/**
 * Euclidean length of a vector.
 */
template <typename T, uint64_t D,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding>
norm(const data<sch::Vector<T, D>, Encoding> &input) {
	return sqrt<T, Encoding>(dot<T, D, Encoding>(input, input));
}

/**
 * Unit length copy of a vector. A zero vector has no direction to preserve, so
 * it is returned unchanged rather than divided by zero.
 */
template <typename T, uint64_t D,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, D>, Encoding>
normalize(const data<sch::Vector<T, D>, Encoding> &input) {
	auto inp_dot = dot<T, D, Encoding>(input, input);
	if (inp_dot.flat_at(0u).get() <= 0) {
		return input;
	}
	auto sqrt_inp_dot = sqrt<T, Encoding>(inp_dot);

	data<sch::Vector<T, D>, Encoding> out;
	for (uint64_t i = 0u; i < D; ++i) {
		out.flat_at(i) = input.flat_at(i) / sqrt_inp_dot.flat_at(0u);
	}
	return out;
}

/**
 * Reinterprets a plain fixed array as a vector, so the algebra below applies
 * to it.
 */
template <typename T, uint64_t D,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, D>, Encoding>
vectorize_data(const data<sch::FixedArray<T, D>, Encoding> &dat) {
	data<sch::Vector<T, D>, Encoding> vec_data;
	for (uint64_t i = 0u; i < D; ++i) {
		vec_data.flat_at(i) = dat.flat_at(i);
	}

	return vec_data;
}

template <typename T, uint64_t M, uint64_t N, uint64_t K,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Matrix<T, M, N>, Encoding>
multiply(const data<sch::Matrix<T, M, K>, Encoding> &left,
		 const data<sch::Matrix<T, K, N>, Encoding> &right) {
	data<sch::Matrix<T, M, N>, Encoding> lr;

	for (uint64_t i = 0u; i < M; ++i) {
		for (uint64_t j = 0u; j < N; ++j) {
			auto &target = lr.at({{i, j}});
			for (uint64_t k = 0u; k < K; ++k) {
				target = target + left.at({{i, k}}) * right.at({{k, j}});
			}
		}
	}

	return lr;
}

template <typename T, uint64_t M, uint64_t N,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, M>, Encoding>
multiply(const data<sch::Matrix<T, M, N>, Encoding> &left,
		 const data<sch::Vector<T, N>, Encoding> &right) {
	data<sch::Vector<T, M>, Encoding> lr;

	for (uint64_t i = 0u; i < M; ++i) {
		auto &target = lr.flat_at(i);
		for (uint64_t j = 0u; j < N; ++j) {
			target = target + left.at({{i, j}}) * right.flat_at(j);
		}
	}

	return lr;
}

template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, 3u>, Encoding>
cross(const data<sch::Vector<T, 3u>, Encoding> &lh,
	  const data<sch::Vector<T, 3u>, Encoding> &rh) {
	data<sch::Vector<T, 3u>, Encoding> cross_prod;

	cross_prod.flat_at(0u) = lh.flat_at(1u) * rh.flat_at(2u) - lh.flat_at(2u) * rh.flat_at(1u);
	cross_prod.flat_at(1u) = lh.flat_at(2u) * rh.flat_at(0u) - lh.flat_at(0u) * rh.flat_at(2u);
	cross_prod.flat_at(2u) = lh.flat_at(0u) * rh.flat_at(1u) - lh.flat_at(1u) * rh.flat_at(0u);

	return cross_prod;
}

/**
 * In two dimensions the cross product has only the component normal to the
 * plane, so it comes back as a scalar.
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding>
cross(const data<sch::Vector<T, 2u>, Encoding> &lh,
	  const data<sch::Vector<T, 2u>, Encoding> &rh) {
	data<sch::Scalar<T>, Encoding> cross_prod;

	cross_prod.flat_at(0u) =
		lh.flat_at(0u) * rh.flat_at(1u) - lh.flat_at(1u) * rh.flat_at(0u);

	return cross_prod;
}

/**
 * Cross product of an out of plane scalar with an in plane vector, which is
 * the rotation of that vector by a quarter turn, scaled.
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, 2u>, Encoding>
cross(const data<sch::Scalar<T>, Encoding> &lh,
	  const data<sch::Vector<T, 2u>, Encoding> &rh) {
	data<sch::Vector<T, 2u>, Encoding> cross_prod;

	cross_prod.flat_at(0u) = -(lh.flat_at(0u) * rh.flat_at(1u));
	cross_prod.flat_at(1u) = lh.flat_at(0u) * rh.flat_at(0u);

	return cross_prod;
}

template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding> cos(const data<sch::Scalar<T>, Encoding> &val) {
	data<sch::Scalar<T>, Encoding> ret;
	ret.flat_at(0u).set(std::cos(val.flat_at(0u).get()));
	return ret;
}

template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding> sin(const data<sch::Scalar<T>, Encoding> &val) {
	data<sch::Scalar<T>, Encoding> ret;
	ret.flat_at(0u).set(std::sin(val.flat_at(0u).get()));
	return ret;
}

/**
 * Angle of a 2d vector measured counter clockwise from the positive x axis.
 * Inverse of unit_vector().
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding>
angle(const data<sch::Vector<T, 2u>, Encoding> &vec) {
	data<sch::Scalar<T>, Encoding> ret;
	ret.flat_at(0u).set(std::atan2(vec.flat_at(1u).get(), vec.flat_at(0u).get()));
	return ret;
}

/**
 * Unit length 2d vector pointing along the given angle. Inverse of angle().
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, 2u>, Encoding>
unit_vector(const data<sch::Scalar<T>, Encoding> &rot) {
	data<sch::Vector<T, 2u>, Encoding> ret;
	ret.flat_at(0u) = cos<T, Encoding>(rot).flat_at(0u);
	ret.flat_at(1u) = sin<T, Encoding>(rot).flat_at(0u);
	return ret;
}

/**
 * Rotates a 2d vector counter clockwise by the given angle.
 */
template <typename T, typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, 2u>, Encoding>
rotate(const data<sch::Vector<T, 2u>, Encoding> &vec,
	   const data<sch::Scalar<T>, Encoding> &rot) {
	data<sch::Vector<T, 2u>, Encoding> rot_vec;

	auto c = cos<T, Encoding>(rot).flat_at(0u);
	auto s = sin<T, Encoding>(rot).flat_at(0u);

	rot_vec.flat_at(0u) = vec.flat_at(0u) * c - vec.flat_at(1u) * s;
	rot_vec.flat_at(1u) = vec.flat_at(0u) * s + vec.flat_at(1u) * c;

	return rot_vec;
}

template <typename T, uint64_t D,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Vector<T, D>, Encoding>
scale(const data<sch::Vector<T, D>, Encoding> &vec,
	  const data<sch::Scalar<T>, Encoding> &scale_) {
	data<sch::Vector<T, D>, Encoding> sc_vec;
	for (uint64_t i = 0u; i < D; ++i) {
		sc_vec.flat_at(i) = vec.flat_at(i) * scale_.flat_at(0u);
	}
	return sc_vec;
}

template <typename T, uint64_t... Ds,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
void fill(data<sch::Tensor<T, Ds...>, Encoding> &tbf,
		  const data<T, Encoding> &filler) {
	constexpr uint64_t flat = ct_multiply<uint64_t, Ds...>::value;
	for (uint64_t i = 0u; i < flat; ++i) {
		tbf.flat_at(i) = filler;
	}
}

template <typename T, uint64_t... Ds,
		  typename Encoding = KEL_LBM_DEFAULT_DATA_ENCODING>
data<sch::Scalar<T>, Encoding>
sum(const data<sch::Tensor<T, Ds...>, Encoding> &tens) {
	data<sch::Scalar<T>, Encoding> s;

	constexpr uint64_t flat = ct_multiply<uint64_t, Ds...>::value;
	for (uint64_t i = 0u; i < flat; ++i) {
		s.flat_at(0u) = s.flat_at(0u) + tens.flat_at(i);
	}

	return s;
}

} // namespace math
} // namespace lbm
} // namespace kel
