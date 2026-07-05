#pragma once

#include "common.hpp"
#include "chunk.hpp"

namespace kel {
namespace lbm {

namespace impl {

template<typename Sch>
struct memory_size_helper;

template<typename Sch, uint64_t N>
struct memory_size_helper<sch::Primitive<Sch,N>> {
	static constexpr uint64_t bytes = N;
};

template<typename Sch, uint64_t... N>
struct memory_size_helper<sch::FixedArray<Sch,N...>> {
	static constexpr uint64_t bytes = saw::ct_multiply<uint64_t,N...>::value * memory_size_helper<Sch>::bytes;
};

template<typename Sch, uint64_t... N>
struct memory_size_helper<sch::Tensor<Sch,N...>> {
	static constexpr uint64_t bytes = saw::ct_multiply<uint64_t,N...>::value * memory_size_helper<Sch>::bytes;
};

template<typename Sch, uint64_t Ghost, uint64_t... N>
struct memory_size_helper<sch::Chunk<Sch,Ghost,N...>> {
	static constexpr uint64_t bytes = memory_size_helper<typename sch::Chunk<Sch,Ghost,N...>::InnerSchema>::bytes;
};

template<typename... Members>
struct memory_size_helper<sch::Struct<Members...>> {

	template<uint64_t i>
	static constexpr uint64_t apply_i() {
		if constexpr ( i < sizeof...(Members) ){
			using M_I = typename saw::parameter_pack_type<i,Members...>::type;
			return apply_i<i+1u>() + memory_size_helper<typename M_I::ValueType>::bytes;
		}
		return 0u;
	}

	static constexpr uint64_t bytes = apply_i<0u>();
};

template<typename... T>
class memory_estimate_helper final {
private:

	template<uint64_t i>
	static void apply_i(saw::data<sch::UInt64>& bytes){

		if constexpr ( i < sizeof...(T)){
			using T_I = typename saw::parameter_pack_type<i,T...>::type;

			bytes.set(bytes.get() + memory_size_helper<T_I>::bytes);

			apply_i<i+1u>(bytes);
		}
	}

public:

	static void apply(saw::data<sch::UInt64>& bytes){
		apply_i<0u>(bytes);
	}
};
}

template<typename... T>
saw::data<sch::UInt64> memory_estimate(){
	saw::data<sch::UInt64> bytes;

	impl::memory_estimate_helper<T...>::apply(bytes);

	return bytes;
}
}
}
