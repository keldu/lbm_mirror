#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace encode {
template<typename Encode>
struct Sycl {
};
}

namespace impl {
template<typename Schema>
struct struct_has_only_equal_dimension_array
}
}
}

namespace saw {
template<uint64_t... Meta, typename... Sch, string_literal... Keys, typename Encode>
class data<schema::Struct<schema::Member<schema::FixedArray<Sch,Meta...>, Keys>...>, kel::lbm::encode::Sycl<Encode>> final {
public:
	static constexpr data<schema::FixedArray meta = {{Meta...}};
	using StorageT = std::tuple<data<Members::Type::InnerSchema,Encode>*...>;
private:

	/**
	 * @todo Check by static assert that the members all have the same dimensions. Alternatively
	 * Do it here by specializing.
	 */
	StorageT members_;
	kel::lbm::sycl::queue* q_;
public:
	data():
		members_{},
		q_{nullptr}
	{}

	~data(){
		SAW_ASSERT(q_){
			exit(-1);
		}
		std::visit([this](auto arg){
			if(not arg){
				return;
			}
			sycl::free(arg,*q_);
			arg = nullptr;
		},members_);
	}

	template<saw::string_literal K>
	auto* get_ptr(){
		return std::get<parameter_key_pack_index<K, Keys...>::value(members_);
	}

	template<saw::string_literal K>
	auto& get(){
		auto ptr = get_ptr<K>();
		SAW_ASSERT(ptr);
		return *ptr;
	}

	void set_queue(kel::lbm::sycl::queue& q){
		q_ = &q;
	}
};

}

namespace kel {
namespace lbm {
namespace impl {
template<typename Sch, typename Encode>
struct sycl_malloc_struct_helper;
	
template<typename... Members, typename Encode>
struct sycl_malloc_struct_helper<sch::Struct<Members...>, Encode> final {
	template<uint64_t i>
	static saw::error_or<void> allocate_on_device_member(typename data<Sch,encode::Sycl<Encode>>::StorageT& storage, sycl::queue& q){
		if constexpr (i < sizeof...(Members)){
			auto& ptr = std::get<i>(storage);

			return allocate_on_device_member<i+1u>(sycl_data,q);
		}
		
		return saw::make_void();
	}
	
	static saw::error_or<void> allocate_on_device(data<Sch,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		typename data<Sch,encode::Sycl<Encode>>::StorageT storage;
		return allocate_on_device_member<0u>(storage,q);
	}
};
}
class device final {
private:
	sycl::queue q_;

	SAW_FORBID_COPY(device);
	SAW_FORBID_MOVE(device);
public:
	device() = default;
	~device() = default;

	template<typename Sch, typename Encode>
	saw::error_or<void> allocate_on_device(data<Sch,encode::Sycl<Encode>>& sycl_data){
		auto eov = sycl_malloc_struct_helper<Sch,Encode>::allocate_on_device(sycl_data, q_);
		if(eov.is_error()){
			return eov;
		}
		sycl_data.set_queue(q_);
	}
};
}
}
