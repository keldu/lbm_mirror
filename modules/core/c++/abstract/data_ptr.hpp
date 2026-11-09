#pragma once

#include "data.hpp"
#include "data_math.hpp"

namespace kel {
namespace lbm {

/**
 * Host-side borrowed views, mirroring the device Ptr encodings.
 *
 * On an accelerator a kernel captures its arguments by value into a const
 * lambda, so a device view is always reached through a const object. The
 * device types answer that by being a const POINTER to mutable data -- the
 * same distinction as `T* const` against `const T*`. That is the correct model
 * for a borrowed device handle, not a workaround.
 *
 * The consequence is that components, which take their group through such a
 * view, cannot be instantiated against the plain host `data` types, because
 * those are honestly const-correct. Templates are only checked when
 * instantiated, so a component referencing a member that does not exist
 * compiles fine until a real kernel is built -- which is how two such bugs got
 * past the host tests here already.
 *
 * These views close that hole: same const semantics as the device, usable on
 * the host, so a component can be instantiated and exercised in a plain test.
 */

template <typename Sch, uint64_t... Dims>
class data<sch::Ptr<sch::FixedArray<Sch, Dims...>>, enc::Native> final {
public:
	using Schema = sch::Ptr<sch::FixedArray<Sch, Dims...>>;
	using Inner = data<Sch, enc::Native>;

	static constexpr uint64_t Rank = sizeof...(Dims);

private:
	Inner *values_;

public:
	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data() : values_{nullptr} {}

	data(data<sch::FixedArray<Sch, Dims...>, enc::Native> &backing)
		: values_{backing.flat_data()} {}

	data(Inner *values__) : values_{values__} {}

	static constexpr index_data<Rank> get_dims() { return {Dims...}; }

	static constexpr index_data<Rank> meta() { return {Dims...}; }

	/**
	 * Const on the view, mutable through it. Matches the device encoding.
	 */
	Inner &at(const index_data<Rank> &index) const {
		// Row major, same fold the owning FixedArray uses.
		constexpr std::array<uint64_t, Rank> dims{Dims...};
		uint64_t flat = 0u;
		uint64_t stride = 1u;
		for(uint64_t i{0u}; i < Rank; ++i){
			flat += index.flat_at(i).get() * stride;
			stride *= dims.at(i);
		}
		return values_[flat];
	}

	Inner &flat_at(uint64_t i) const { return values_[i]; }

	Inner *flat_data() const { return values_; }
};

template <typename... Sch, string_literal... Keys>
class data<sch::Ptr<sch::Struct<sch::Member<Sch, Keys>...>>, enc::Native> final {
public:
	using Schema = sch::Ptr<sch::Struct<sch::Member<Sch, Keys>...>>;

private:
	using StorageT = std::tuple<data<sch::Ptr<Sch>, enc::Native>...>;
	StorageT members_;

	template <string_literal K>
	static constexpr size_t index_of = parameter_key_pack_index<K, Keys...>::value;

public:
	data() = default;

	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	/**
	 * Builds a view over an owning struct, member by member.
	 */
	data(data<sch::Struct<sch::Member<Sch, Keys>...>, enc::Native> &backing) {
		bind<0u>(backing);
	}

	template <size_t i> auto &get() const {
		return std::get<i>(const_cast<StorageT &>(members_));
	}

	template <string_literal K> auto &get() const {
		return std::get<index_of<K>>(const_cast<StorageT &>(members_));
	}

private:
	template <uint64_t i, typename Backing> void bind(Backing &backing) {
		if constexpr (i < sizeof...(Sch)) {
			std::get<i>(members_) = {backing.template get_at_index<i>()};
			bind<i + 1u>(backing);
		}
	}
};

/**
 * Non-structural leaves are borrowed as a plain pointer to the value.
 */
template <typename Sch>
class data<sch::Ptr<Sch>, enc::Native> final {
public:
	using Schema = sch::Ptr<Sch>;
	using Inner = data<Sch, enc::Native>;

private:
	Inner *value_;

public:
	KEL_DEFAULT_COPY(data);
	KEL_DEFAULT_MOVE(data);

	data() : value_{nullptr} {}
	data(Inner &value__) : value_{&value__} {}

	Inner &operator()() const { return *value_; }

	Inner *handle() const { return value_; }
};

/**
 * Convenience: build a host view over an owning value, mirroring make_view.
 */
template <typename Sch>
data<sch::Ptr<Sch>, enc::Native> make_host_view(data<Sch, enc::Native> &backing) {
	return data<sch::Ptr<Sch>, enc::Native>{backing};
}

} // namespace lbm
} // namespace kel
