#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <cassert>

#include "common.hpp"
#include "encode.hpp"
#include "error.hpp"
#include "schema.hpp"
#include "templates.hpp"

namespace kel {
namespace lbm {

/**
 * Maps a Primitive schema onto the C++ type that actually holds it.
 */
template <class T> struct native_data_type;

template <> struct native_data_type<sch::Primitive<sch::SignedInteger, 1>> {
	using type = int8_t;
};
template <> struct native_data_type<sch::Primitive<sch::SignedInteger, 2>> {
	using type = int16_t;
};
template <> struct native_data_type<sch::Primitive<sch::SignedInteger, 4>> {
	using type = int32_t;
};
template <> struct native_data_type<sch::Primitive<sch::SignedInteger, 8>> {
	using type = int64_t;
};

template <> struct native_data_type<sch::Primitive<sch::UnsignedInteger, 1>> {
	using type = uint8_t;
};
template <> struct native_data_type<sch::Primitive<sch::UnsignedInteger, 2>> {
	using type = uint16_t;
};
template <> struct native_data_type<sch::Primitive<sch::UnsignedInteger, 4>> {
	using type = uint32_t;
};
template <> struct native_data_type<sch::Primitive<sch::UnsignedInteger, 8>> {
	using type = uint64_t;
};

template <> struct native_data_type<sch::Primitive<sch::FloatingPoint, 4>> {
	using type = float;
};
template <> struct native_data_type<sch::Primitive<sch::FloatingPoint, 8>> {
	using type = double;
};

/**
 * MixedPrecision reads as its interface type, so that is what a caller sees.
 */
template <typename S, typename I>
struct native_data_type<sch::MixedPrecision<S, I>> {
	using type = typename native_data_type<I>::type;
};

template <typename Schema, typename Encode = KEL_LBM_DEFAULT_DATA_ENCODING>
class data {
	static_assert(always_false<Schema, Encode>,
				  "Invalid schema + encode in data specialization");
};

/**
 * The index type used to address a shape of the given rank.
 */
template <uint64_t Rank>
using index_data = data<sch::FixedArray<sch::UInt64, Rank>, enc::Native>;

template <> class data<sch::Void, enc::Native> final {
public:
	using Schema = sch::Void;
	using MetaSchema = typename Schema::Meta;

	constexpr data() = default;
};

template <> class data<sch::Bool, enc::Native> final {
public:
	using Schema = sch::Bool;
	using MetaSchema = typename Schema::Meta;

private:
	bool value_;

public:
	constexpr data() : value_{false} {}
	constexpr data(data<MetaSchema, enc::Native>) : value_{false} {}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	constexpr data(bool value__) : value_{value__} {}

	constexpr void set(bool val) { value_ = val; }

	constexpr bool get() const { return value_; }

	template <typename Enc = enc::Native>
	constexpr bool equals(const data<Schema, Enc> &rhs) const {
		return get() == rhs.get();
	}

	template <typename Enc>
	constexpr bool operator==(const data<Schema, Enc> &rhs) const {
		return equals(rhs);
	}

	template <typename Enc>
	constexpr bool operator!=(const data<Schema, Enc> &rhs) const {
		return get() != rhs.get();
	}

	constexpr data<Schema, enc::Native> operator!() const { return {!get()}; }

	template <typename Enc>
	constexpr data<Schema, enc::Native>
	operator&&(const data<Schema, Enc> &rhs) const {
		return {get() && rhs.get()};
	}

	template <typename Enc>
	constexpr data<Schema, enc::Native>
	operator||(const data<Schema, Enc> &rhs) const {
		return {get() || rhs.get()};
	}

	template <typename Target>
	constexpr data<Target, enc::Native> cast_to() const {
		return {static_cast<typename native_data_type<Target>::type>(value_)};
	}
};

template <typename T, uint64_t N>
class data<sch::Primitive<T, N>, enc::Native> final {
public:
	using Schema = sch::Primitive<T, N>;
	using MetaSchema = typename Schema::Meta;
	using NativeType = typename native_data_type<Schema>::type;

private:
	NativeType value_;

public:
	constexpr data() : value_{} {}
	constexpr data(data<MetaSchema, enc::Native>) : value_{} {}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	constexpr data(NativeType value__) : value_{std::move(value__)} {}

	constexpr void set(NativeType val) { value_ = val; }

	constexpr NativeType get() const { return value_; }

	constexpr data<Schema, enc::Native>
	operator*(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() * rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator/(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() / rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator+(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() + rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator-(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() - rhs.get())};
	}

	constexpr data<Schema, enc::Native> operator-() const {
		return {static_cast<NativeType>(-get())};
	}

	constexpr data<Schema, enc::Native> &
	operator+=(const data<Schema, enc::Native> &rhs) {
		value_ = static_cast<NativeType>(value_ + rhs.get());
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator-=(const data<Schema, enc::Native> &rhs) {
		value_ = static_cast<NativeType>(value_ - rhs.get());
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator*=(const data<Schema, enc::Native> &rhs) {
		value_ = static_cast<NativeType>(value_ * rhs.get());
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator/=(const data<Schema, enc::Native> &rhs) {
		value_ = static_cast<NativeType>(value_ / rhs.get());
		return *this;
	}

	constexpr data<Schema, enc::Native> &operator++() {
		set(static_cast<NativeType>(get() + static_cast<NativeType>(1)));
		return *this;
	}

	template <typename Enc = enc::Native>
	constexpr bool equals(const data<Schema, Enc> &rhs) const {
		return get() == rhs.get();
	}

	template <typename Enc>
	constexpr bool operator==(const data<Schema, Enc> &rhs) const {
		return equals(rhs);
	}

	template <typename Enc>
	constexpr bool operator!=(const data<Schema, Enc> &rhs) const {
		return get() != rhs.get();
	}

	template <typename Enc>
	constexpr bool operator<(const data<Schema, Enc> &rhs) const {
		return get() < rhs.get();
	}

	template <typename Enc>
	constexpr bool operator>(const data<Schema, Enc> &rhs) const {
		return get() > rhs.get();
	}

	template <typename Enc>
	constexpr bool operator<=(const data<Schema, Enc> &rhs) const {
		return get() <= rhs.get();
	}

	template <typename Enc>
	constexpr bool operator>=(const data<Schema, Enc> &rhs) const {
		return get() >= rhs.get();
	}

	template <typename Target>
	constexpr data<Target, enc::Native> cast_to() const {
		return {static_cast<typename native_data_type<Target>::type>(value_)};
	}
};

/**
 * Stored narrow, read wide. The stored value is kept as StorageType and every
 * read widens it to InterfaceType, so arithmetic happens at interface
 * precision and only the resting representation is narrow.
 */
template <typename StorageT, typename InterfaceT>
class data<sch::MixedPrecision<StorageT, InterfaceT>, enc::Native> final {
public:
	using Schema = sch::MixedPrecision<StorageT, InterfaceT>;
	using MetaSchema = typename Schema::Meta;
	using StorageType = StorageT;
	using InterfaceType = InterfaceT;
	using NativeType = typename native_data_type<InterfaceT>::type;

private:
	data<StorageT, enc::Native> value_;

	static constexpr typename native_data_type<StorageT>::type
	narrow(NativeType val) {
		return static_cast<typename native_data_type<StorageT>::type>(val);
	}

public:
	constexpr data() : value_{} {}
	constexpr data(data<MetaSchema, enc::Native>) : value_{} {}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	constexpr data(NativeType val__) : value_{narrow(val__)} {}

	constexpr data(const data<InterfaceT, enc::Native> &val)
		: value_{val.template cast_to<StorageT>()} {}

	constexpr NativeType get() const {
		return value_.template cast_to<InterfaceT>().get();
	}

	constexpr void set(NativeType val) { value_.set(narrow(val)); }

	constexpr data<Schema, enc::Native>
	operator*(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() * rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator/(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() / rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator+(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() + rhs.get())};
	}

	constexpr data<Schema, enc::Native>
	operator-(const data<Schema, enc::Native> &rhs) const {
		return {static_cast<NativeType>(get() - rhs.get())};
	}

	constexpr data<Schema, enc::Native> operator-() const {
		return {static_cast<NativeType>(-get())};
	}

	constexpr data<Schema, enc::Native> &
	operator+=(const data<Schema, enc::Native> &rhs) {
		set(static_cast<NativeType>(get() + rhs.get()));
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator-=(const data<Schema, enc::Native> &rhs) {
		set(static_cast<NativeType>(get() - rhs.get()));
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator*=(const data<Schema, enc::Native> &rhs) {
		set(static_cast<NativeType>(get() * rhs.get()));
		return *this;
	}

	constexpr data<Schema, enc::Native> &
	operator/=(const data<Schema, enc::Native> &rhs) {
		set(static_cast<NativeType>(get() / rhs.get()));
		return *this;
	}

	template <typename Enc>
	constexpr bool operator==(const data<Schema, Enc> &rhs) const {
		return get() == rhs.get();
	}

	template <typename Enc>
	constexpr bool operator!=(const data<Schema, Enc> &rhs) const {
		return get() != rhs.get();
	}

	template <typename Enc>
	constexpr bool operator<(const data<Schema, Enc> &rhs) const {
		return get() < rhs.get();
	}

	template <typename Enc>
	constexpr bool operator>(const data<Schema, Enc> &rhs) const {
		return get() > rhs.get();
	}

	template <typename Target>
	constexpr data<Target, enc::Native> cast_to() const {
		return {static_cast<typename native_data_type<Target>::type>(get())};
	}
};

/**
 * Array whose extents are known at compile time.
 *
 * A rank zero FixedArray holds exactly one element, because the empty product
 * is one. That is what makes Scalar work without a special case.
 */
template <typename T, uint64_t... D>
class data<sch::FixedArray<T, D...>, enc::Native> final {
public:
	using Schema = sch::FixedArray<T, D...>;
	using MetaSchema = typename Schema::Meta;
	using Inner = data<T, enc::Native>;

	static constexpr uint64_t Rank = sizeof...(D);
	static constexpr uint64_t FlatSize = ct_multiply<uint64_t, D...>::value;

private:
	using ArrayT = std::array<Inner, FlatSize>;
	ArrayT value_;

	static constexpr std::array<uint64_t, Rank> dims_{D...};

	constexpr uint64_t get_flat_index(const index_data<Rank> &i) const {
		uint64_t s = 0;
		uint64_t stride = 1;

		for (uint64_t iter = 0; iter < Rank; ++iter) {
			assert(i.flat_at(iter).get() < dims_.at(iter));
			s += i.flat_at(iter).get() * stride;
			stride *= dims_.at(iter);
		}

		return s;
	}

public:
	constexpr data() = default;
	constexpr data(data<MetaSchema, enc::Native>)
		requires(not std::is_same_v<MetaSchema, Schema>)
	{}

	constexpr data(const ArrayT &value__) : value_{value__} {}

	/**
	 * Forwarding constructor, so the elements can be written without spelling
	 * out the inner array: data<FixedArray<UInt64,3>>{0,1,2}.
	 *
	 * The guards keep it clear of the copy constructor and of the metadata
	 * constructor.
	 *
	 * It is deliberately implicit even for a single element. That is what lets
	 * a rank one shape be addressed as at({i}) as well as at({{i}}): the index
	 * is built here rather than through a separate data<UInt64> overload.
	 * Offering both spellings as overloads instead makes at({{i}}) ambiguous,
	 * since a one element list converts equally well to either.
	 */
	template <typename... Args>
		requires(sizeof...(Args) == FlatSize) &&
				(sizeof...(Args) > 1u ||
				 (!std::same_as<std::remove_cvref_t<Args>, data> && ...)) &&
				(!std::same_as<std::remove_cvref_t<Args>,
							   data<MetaSchema, enc::Native>> &&
				 ...) &&
				(std::constructible_from<Inner, Args> && ...)
	constexpr data(Args &&...args)
		: value_{Inner{std::forward<Args>(args)}...} {}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	constexpr Inner &at(const index_data<Rank> &i) {
		return value_.at(this->get_flat_index(i));
	}

	constexpr const Inner &at(const index_data<Rank> &i) const {
		return value_.at(this->get_flat_index(i));
	}

	constexpr Inner &operator()(const index_data<Rank> &i) {
		return value_.at(this->get_flat_index(i));
	}

	constexpr const Inner &operator()(const index_data<Rank> &i) const {
		return value_.at(this->get_flat_index(i));
	}

	/**
	 * Rank agnostic access by position in the flattened storage.
	 *
	 * This is what lets a rank N index array be walked element by element.
	 * at() cannot serve that purpose, since for rank other than one a bare
	 * integer is not a valid index into this shape.
	 */
	constexpr Inner &flat_at(uint64_t i) { return value_.at(i); }

	constexpr const Inner &flat_at(uint64_t i) const { return value_.at(i); }

	template <uint64_t i> static constexpr uint64_t get_dim_size() {
		static_assert(i < Rank, "Dimension index is too large for this shape");
		return parameter_pack_value<i, uint64_t, D...>::value;
	}

	static constexpr uint64_t get_dim_size(uint64_t i) { return dims_.at(i); }

	static constexpr index_data<Rank> get_dims() { return {D...}; }

	static constexpr data<sch::UInt64, enc::Native> flat_dims() {
		return {FlatSize};
	}

	static constexpr data<sch::UInt64, enc::Native> size() {
		return {FlatSize};
	}

	constexpr Inner *flat_data() {
		if constexpr (FlatSize == 0u) {
			return nullptr;
		} else {
			return &value_[0u];
		}
	}

	constexpr const Inner *flat_data() const {
		if constexpr (FlatSize == 0u) {
			return nullptr;
		} else {
			return &value_[0u];
		}
	}

	template <typename Enc>
	constexpr bool operator==(const data<Schema, Enc> &rhs) const {
		for (uint64_t i = 0; i < FlatSize; ++i) {
			if (!(flat_at(i) == rhs.flat_at(i))) {
				return false;
			}
		}
		return true;
	}
};

/**
 * Array whose extents are only known at runtime. The extents are the metadata,
 * so constructing one from its MetaSchema is what gives it a shape.
 */
template <typename T, uint64_t Dim>
class data<sch::Array<T, Dim>, enc::Native> final {
public:
	using Schema = sch::Array<T, Dim>;
	using MetaSchema = typename Schema::Meta;
	using Inner = data<T, enc::Native>;

	static constexpr uint64_t Rank = Dim;

private:
	index_data<Dim> dims_;
	std::vector<Inner> value_;

	constexpr uint64_t get_full_size() const {
		uint64_t s = 1;
		for (uint64_t iter = 0; iter < Dim; ++iter) {
			s *= dims_.flat_at(iter).get();
		}
		return s;
	}

	uint64_t get_flat_index(const index_data<Dim> &i) const {
		assert(value_.size() == get_full_size());
		uint64_t s = 0;
		uint64_t stride = 1;

		for (uint64_t iter = 0; iter < Dim; ++iter) {
			uint64_t ind = i.flat_at(iter).get();
			assert(ind < dims_.flat_at(iter).get());
			s += ind * stride;
			stride *= dims_.flat_at(iter).get();
		}

		return s;
	}

public:
	data() : dims_{}, value_{} {
		for (uint64_t iter = 0; iter < Dim; ++iter) {
			dims_.flat_at(iter).set(0u);
		}
	}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data(const data<MetaSchema, enc::Native> &init__)
		: dims_{init__}, value_{} {
		value_.resize(get_full_size());
	}

	template <std::integral... Dims>
		requires(sizeof...(Dims) == Dim)
	data(Dims... size_)
		: data{index_data<Dim>{static_cast<uint64_t>(size_)...}} {}

	template <uint64_t i = 0> error_or<void> add(Inner val) {
		/**
		 * Only the last dimension can grow without moving anything, since it
		 * is the one that does not take part in the stride. Higher dimensional
		 * growth would mean restriding the whole buffer, so it is left out.
		 */
		static_assert(Dim == 1, "Currently can't deal with higher dims");
		static_assert(i < Dim,
					  "Can't add to dimension. Index i larger than dimension size");

		try {
			value_.emplace_back(std::move(val));
		} catch (const std::exception &e) {
			(void)e;
			return make_error<err::out_of_memory>();
		}

		++dims_.flat_at(i);

		return make_void();
	}

	Inner &at(const index_data<Dim> &i) {
		return value_.at(this->get_flat_index(i));
	}

	const Inner &at(const index_data<Dim> &i) const {
		return value_.at(this->get_flat_index(i));
	}

	Inner &operator()(const index_data<Dim> &i) {
		return value_.at(this->get_flat_index(i));
	}

	const Inner &operator()(const index_data<Dim> &i) const {
		return value_.at(this->get_flat_index(i));
	}

	Inner &flat_at(uint64_t i) { return value_.at(i); }

	const Inner &flat_at(uint64_t i) const { return value_.at(i); }

	template <uint64_t i>
	constexpr data<sch::UInt64, enc::Native> get_dim_size() const {
		static_assert(i < Dim, "Dimension index is too large for this shape");
		return dims_.flat_at(i);
	}

	constexpr std::size_t get_dim_size(uint64_t i) const {
		return dims_.flat_at(i).get();
	}

	constexpr data<sch::UInt64, enc::Native> size() const {
		return {value_.size()};
	}

	index_data<Dim> get_dims() const { return dims_; }

	/**
	 * The metadata of a value is whatever it would take to rebuild its shape.
	 */
	index_data<Dim> meta() const { return dims_; }

	index_data<Dim> dims() const { return dims_; }

	data<sch::UInt64, enc::Native> flat_size() const {
		return {value_.size()};
	}

	Inner *flat_data() {
		if (value_.size() == 0u) {
			return nullptr;
		}
		return &value_[0u];
	}

	const Inner *flat_data() const {
		if (value_.size() == 0u) {
			return nullptr;
		}
		return &value_[0u];
	}
};

/**
 * Variable length string. Its length is its metadata.
 */
template <> class data<sch::String, enc::Native> final {
public:
	using Schema = sch::String;
	using MetaSchema = typename Schema::Meta;

private:
	std::string value_;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data(data<MetaSchema, enc::Native> init) { value_.resize(init.get()); }

	data(std::string value__) : value_{std::move(value__)} {}

	data(const char *value__) : value_{value__} {}

	data<sch::UInt64, enc::Native> size() const { return {value_.size()}; }

	void set(std::string str) { value_ = std::move(str); }

	char &at(data<sch::UInt64, enc::Native> i) { return value_.at(i.get()); }

	const char &at(data<sch::UInt64, enc::Native> i) const {
		return value_.at(i.get());
	}

	char get_at(data<sch::UInt64, enc::Native> i) const {
		return value_.at(i.get());
	}

	void set_at(data<sch::UInt64, enc::Native> i, char val) {
		value_.at(i.get()) = val;
	}

	std::string_view stl_view() { return value_; }

	std::string_view stl_view() const { return value_; }

	std::string stl_string() const { return value_; }

	template <typename Enc>
	bool operator==(const data<sch::String, Enc> &rhs) const {
		return stl_view() == rhs.stl_view();
	}

	template <typename Enc>
	bool operator!=(const data<sch::String, Enc> &rhs) const {
		return stl_view() != rhs.stl_view();
	}

	template <typename Enc>
	bool operator<(const data<sch::String, Enc> &rhs) const {
		return stl_view() < rhs.stl_view();
	}
};

template <typename... T> class data<sch::Tuple<T...>, enc::Native> final {
public:
	using Schema = sch::Tuple<T...>;
	using MetaSchema = typename Schema::Meta;

private:
	std::tuple<data<T, enc::Native>...> value_;

	template <uint64_t i = 0u> struct init_helper {
		static void apply(std::tuple<data<T, enc::Native>...> &val,
						  const data<MetaSchema, enc::Native> &init) {
			std::get<i>(val) = {init.template get<i>()};

			if constexpr ((i + 1u) < sizeof...(T)) {
				init_helper<i + 1u>::apply(val, init);
			}
		}
	};

public:
	data() = default;

	data(const data<MetaSchema, enc::Native> &init)
		requires(not std::is_same_v<MetaSchema, Schema>)
	{
		if constexpr (0u < sizeof...(T)) {
			init_helper<0u>::apply(value_, init);
		}
	}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	template <size_t i>
	data<typename parameter_pack_type<i, T...>::type, enc::Native> &get() {
		return std::get<i>(value_);
	}

	template <size_t i>
	const data<typename parameter_pack_type<i, T...>::type, enc::Native> &
	get() const {
		return std::get<i>(value_);
	}

	static constexpr uint64_t size() { return sizeof...(T); }
};

template <typename... T, string_literal... literals>
class data<sch::Struct<sch::Member<T, literals>...>, enc::Native> final {
public:
	using Schema = sch::Struct<sch::Member<T, literals>...>;
	using MetaSchema = typename Schema::Meta;

private:
	std::tuple<data<T, enc::Native>...> value_;

	template <uint64_t i = 0u> struct init_helper {
		static void apply(std::tuple<data<T, enc::Native>...> &val,
						  const data<MetaSchema, enc::Native> &init) {
			std::get<i>(val) = {
				init.template get<parameter_key_pack_type<i, literals...>::literal>()};

			if constexpr ((i + 1u) < sizeof...(T)) {
				init_helper<i + 1u>::apply(val, init);
			}
		}
	};

	template <string_literal literal>
	static constexpr size_t index_of =
		parameter_key_pack_index<literal, literals...>::value;

public:
	data() = default;

	data(const data<MetaSchema, enc::Native> &init)
		requires(not std::is_same_v<MetaSchema, Schema>)
	{
		if constexpr (0u < sizeof...(T)) {
			init_helper<0u>::apply(value_, init);
		}
	}

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	template <string_literal literal>
	data<typename parameter_pack_type<index_of<literal>, T...>::type,
		 enc::Native> &
	get() {
		return std::get<index_of<literal>>(value_);
	}

	template <string_literal literal>
	const data<typename parameter_pack_type<index_of<literal>, T...>::type,
			   enc::Native> &
	get() const {
		return std::get<index_of<literal>>(value_);
	}

	template <size_t i>
	data<typename parameter_pack_type<i, T...>::type, enc::Native> &
	get_at_index() {
		return std::get<i>(value_);
	}

	template <size_t i>
	const data<typename parameter_pack_type<i, T...>::type, enc::Native> &
	get_at_index() const {
		return std::get<i>(value_);
	}

	static constexpr data<sch::UInt64, enc::Native> size() {
		return {sizeof...(T)};
	}
};

template <typename... T, string_literal... literals>
class data<sch::Union<sch::Member<T, literals>...>, enc::Native> final {
public:
	using Schema = sch::Union<sch::Member<T, literals>...>;
	using MetaSchema = typename Schema::Meta;

private:
	std::variant<data<T, enc::Native>...> value_;

	template <string_literal literal>
	static constexpr size_t index_of =
		parameter_key_pack_index<literal, literals...>::value;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	template <string_literal lit>
	void set(data<typename parameter_pack_type<index_of<lit>, T...>::type,
				  enc::Native>
				 val) {
		value_ = std::move(val);
	}

	template <string_literal lit>
	data<typename parameter_pack_type<index_of<lit>, T...>::type, enc::Native> &
	init() {
		value_.template emplace<index_of<lit>>();
		return get<lit>();
	}

	template <string_literal lit> bool holds_alternative() const {
		return index_of<lit> == value_.index();
	}

	template <string_literal lit>
	data<typename parameter_pack_type<index_of<lit>, T...>::type, enc::Native> &
	get() {
		return std::get<index_of<lit>>(value_);
	}

	template <string_literal lit>
	const data<typename parameter_pack_type<index_of<lit>, T...>::type,
			   enc::Native> &
	get() const {
		return std::get<index_of<lit>>(value_);
	}

	constexpr data<sch::UInt64, enc::Native> index() const {
		return {value_.index()};
	}
};

template <typename Key, typename Value>
class data<sch::Map<Key, Value>, enc::Native> final {
public:
	using Schema = sch::Map<Key, Value>;
	using MetaSchema = typename Schema::Meta;

private:
	std::map<data<Key, enc::Native>, data<Value, enc::Native>> vals_;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data(data<MetaSchema, enc::Native>) {}

	error_or<void> emplace(ref<data<Key, enc::Native>> key,
						   ref<data<Value, enc::Native>> value) {
		auto insert = vals_.emplace(std::make_pair(key(), value()));
		if (!insert.second) {
			return make_error<err::already_exists>();
		}
		return make_void();
	}

	error_or<void> erase(ref<data<Key, enc::Native>> key) {
		auto erased = vals_.erase(key());
		if (erased == 0u) {
			return make_error<err::not_found>();
		}
		return make_void();
	}

	error_or<ptr<data<Value, enc::Native>>> find(ref<data<Key, enc::Native>> key) {
		auto found = vals_.find(key());
		if (found == vals_.end()) {
			return make_error<err::not_found>();
		}

		return ptr<data<Value, enc::Native>>{found->second};
	}

	data<sch::UInt64, enc::Native> size() const { return {vals_.size()}; }
};

} // namespace lbm
} // namespace kel
