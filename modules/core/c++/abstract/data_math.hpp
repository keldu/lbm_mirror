#pragma once

#include <cmath>

#include "data.hpp"
#include "iterator.hpp"

namespace kel {
namespace lbm {

/**
 * Tensor of compile time extents over `Inner`.
 *
 * Storage is a FixedArray of the same extents; the tensor adds the algebra on
 * top. Rank zero is a scalar and still holds one element.
 */
template <typename Inner, uint64_t... Dims>
class data<sch::Tensor<Inner, Dims...>, enc::Native> final {
public:
	using Schema = sch::Tensor<Inner, Dims...>;
	using MetaSchema = typename Schema::Meta;
	using InnerData = data<Inner, enc::Native>;
	using Storage = data<sch::FixedArray<Inner, Dims...>, enc::Native>;

	static constexpr uint64_t Rank = sizeof...(Dims);
	static constexpr uint64_t FlatSize = ct_multiply<uint64_t, Dims...>::value;

private:
	Storage values_;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data(const InnerData &inner_value__)
		requires(Rank == 0u)
		: values_{inner_value__} {}

	data(const Storage &inner_values__)
		requires(Rank > 0u)
		: values_{inner_values__} {}

	/**
	 * Forwarding constructor, so a tensor can be written as its elements in
	 * row major order: data<Tensor<Float64,2u,2u>>{1.0, 2.0, 3.0, 4.0}.
	 *
	 * The guards keep it clear of the copy constructor and of the two
	 * constructors above in the single element case.
	 */
	template <typename... Args>
		requires(sizeof...(Args) == FlatSize) &&
				(sizeof...(Args) > 1u ||
				 (!std::same_as<std::remove_cvref_t<Args>, data> && ...)) &&
				(!std::same_as<std::remove_cvref_t<Args>, InnerData> && ...) &&
				(!std::same_as<std::remove_cvref_t<Args>, Storage> && ...) &&
				(std::constructible_from<InnerData, Args> && ...)
	data(Args &&...args) : values_{std::forward<Args>(args)...} {}

	InnerData &at(const index_data<Rank> &index) { return values_.at(index); }

	const InnerData &at(const index_data<Rank> &index) const {
		return values_.at(index);
	}

	InnerData &operator()(const index_data<Rank> &index) {
		return values_.at(index);
	}

	const InnerData &operator()(const index_data<Rank> &index) const {
		return values_.at(index);
	}

	InnerData &flat_at(uint64_t i) { return values_.flat_at(i); }

	const InnerData &flat_at(uint64_t i) const { return values_.flat_at(i); }

	static constexpr data<sch::UInt64, enc::Native> size() {
		return {FlatSize};
	}

	data<Schema, enc::Native>
	operator+(const data<Schema, enc::Native> &rhs) const {
		data<Schema, enc::Native> c;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			c.flat_at(i) = flat_at(i) + rhs.flat_at(i);
		}
		return c;
	}

	data<Schema, enc::Native>
	operator-(const data<Schema, enc::Native> &rhs) const {
		data<Schema, enc::Native> c;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			c.flat_at(i) = flat_at(i) - rhs.flat_at(i);
		}
		return c;
	}

	data<Schema, enc::Native> operator-() const {
		data<Schema, enc::Native> c;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			c.flat_at(i) = -flat_at(i);
		}
		return c;
	}

	data<Schema, enc::Native> &operator+=(const data<Schema, enc::Native> &rhs) {
		for (uint64_t i = 0; i < FlatSize; ++i) {
			flat_at(i) += rhs.flat_at(i);
		}
		return *this;
	}

	data<Schema, enc::Native> &operator-=(const data<Schema, enc::Native> &rhs) {
		for (uint64_t i = 0; i < FlatSize; ++i) {
			flat_at(i) -= rhs.flat_at(i);
		}
		return *this;
	}

	data<Schema, enc::Native>
	operator*(const data<sch::Scalar<Inner>, enc::Native> &scal) const {
		data<Schema, enc::Native> c;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			c.flat_at(i) = flat_at(i) * scal.flat_at(0u);
		}
		return c;
	}

	data<Schema, enc::Native>
	operator/(const data<sch::Scalar<Inner>, enc::Native> &scal) const {
		data<Schema, enc::Native> c;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			c.flat_at(i) = flat_at(i) / scal.flat_at(0u);
		}
		return c;
	}

	template <typename Enc>
	bool operator==(const data<Schema, Enc> &rhs) const {
		for (uint64_t i = 0; i < FlatSize; ++i) {
			if (!(flat_at(i) == rhs.flat_at(i))) {
				return false;
			}
		}
		return true;
	}

	template <typename InnerChange>
	data<sch::Tensor<InnerChange, Dims...>, enc::Native> cast_to() const {
		data<sch::Tensor<InnerChange, Dims...>, enc::Native> changed;
		for (uint64_t i = 0; i < FlatSize; ++i) {
			changed.flat_at(i) = flat_at(i).template cast_to<InnerChange>();
		}
		return changed;
	}
};

} // namespace lbm
} // namespace kel
