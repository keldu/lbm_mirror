#include "common.hpp"
#include <kel/lbm/lbm.hpp>

namespace kel {
namespace lbm {
namespace encode {
template<typename Encode>
struct Sycl {
};
}

/*
namespace impl {
template<typename Schema>
struct struct_has_only_equal_dimension_array{};
}
*/
}
}

namespace saw {
template<typename Sch, uint64_t... Dims, typename Encode>
class data<schema::FixedArray<Sch,Dims...>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::FixedArray<Sch,Dims...>;
private:
	acpp::sycl::queue* q_;
	data<Sch,Encode>* values_;

	SAW_FORBID_COPY(data);
	SAW_DEFAULT_MOVE(data);
public:
	data(acpp::sycl::queue& q__):
		q_{&q__},
		values_{nullptr}
	{
		values_ = acpp::sycl::malloc_device<data<Sch,Encode>>(ct_multiply<uint64_t,Dims...>::value,*q_);
		SAW_ASSERT(values_ and q_);
	}

	~data(){
		if(not values_){
			return;
		}
		SAW_ASSERT(q_);

		acpp::sycl::free(values_,*q_);
		values_ = nullptr;
	}

	static constexpr data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>> get_dims() {
		return saw::data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>>{{Dims...}};
	}

	constexpr data<Sch,Encode>& at(data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}
	
	constexpr data<Sch,Encode>& at(data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() {
		return values_;
	}
};

template<typename Sch, uint64_t... Dims, typename Encode>
class data<schema::Ptr<schema::FixedArray<Sch,Dims...>>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ptr<schema::FixedArray<Sch,Dims...>>;
private:
	data<Sch,Encode>* values_;

	SAW_DEFAULT_COPY(data);
	SAW_DEFAULT_MOVE(data);
public:
	data():
		values_{nullptr}
	{}

	data(data<Sch,Encode>* values__):
		values_{values__}
	{}

	static constexpr data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>> get_dims() {
		return saw::data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>>{{Dims...}};
	}

	constexpr data<Sch,Encode>& at(data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}
	
	constexpr data<Sch,Encode>& at(data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() {
		return values_;
	}
};

template<typename Sch, uint64_t Ghost, uint64_t... Sides, typename Encode>
class data<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>,kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = kel::lbm::sch::Chunk<Sch,Ghost,Sides...>;
private:
	using InnerSchema = typename Schema::InnerSchema;
	using ValueSchema = typename InnerSchema::ValueType;

	data<InnerSchema, kel::lbm::encode::Sycl<Encode>> values_;
public:
	data(acpp::sycl::queue& q__):
		values_{q__}
	{}

	data<ValueSchema, kel::lbm::encode::Sycl<Encode>>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	data<ValueSchema, kel::lbm::encode::Sycl<Encode>>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
		return values_.at(index);
	}

	static constexpr auto get_ghost_dims() {
		return data<InnerSchema,kel::lbm::encode::Sycl<Encode>>::get_dims();
	}

	data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return values_.at(ind);
	}

	data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return values_.at(ind);
	}

	static constexpr auto get_dims(){
		return data<schema::FixedArray<schema::UInt64, sizeof...(Sides)>,kel::lbm::encode::Sycl<Encode>>{{Sides...}};
	}

	auto flat_data(){
		return values_.flat_data();
	}

	static constexpr auto flat_size() {
		return data<InnerSchema,kel::lbm::encode::Sycl<Encode>>::flat_size();
	}
};

template<typename Sch, uint64_t Ghost, uint64_t... Sides, typename Encode>
class data<schema::Ptr<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>>,kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ptr<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>>;
private:
	using InnerSchema = typename kel::lbm::sch::Chunk<Sch,Ghost,Sides...>::InnerSchema;
	using ValueSchema = typename InnerSchema::ValueType;
	
	data<schema::Ptr<InnerSchema>,kel::lbm::encode::Sycl<Encode>> values_;

	SAW_DEFAULT_MOVE(data);
	SAW_DEFAULT_COPY(data);
public:
	data():
		values_{nullptr}
	{}

	data(data<Sch,Encode>* values__):
		values_{values__}
	{}

	data<ValueSchema, kel::lbm::encode::Sycl<Encode>>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	data<ValueSchema, kel::lbm::encode::Sycl<Encode>>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
		return values_.at(index);
	}

	static constexpr auto get_ghost_dims() {
		return data<InnerSchema,kel::lbm::encode::Sycl<Encode>>::get_dims();
	}

	data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return values_.at(ind);
	}

	data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return values_.at(ind);
	}

	static constexpr auto get_dims(){
		return data<schema::FixedArray<schema::UInt64, sizeof...(Sides)>,kel::lbm::encode::Sycl<Encode>>{{Sides...}};
	}

	auto flat_data(){
		return values_.flat_data();
	}

	static constexpr auto flat_size() {
		return data<InnerSchema,kel::lbm::encode::Sycl<Encode>>::flat_size();
	}
};

template<typename... Members, typename Encode>
class data<schema::Struct<Members...>, kel::lbm::encode::Sycl<Encode> > final {
public:
	using StorageT = std::tuple<data<typename Members::ValueType,kel::lbm::encode::Sycl<Encode>>...>;
	using Schema = schema::Struct<Members...>;
private:
	/**
	 * @todo Check by static assert that the members all have the same dimensions. Alternatively
	 * Do it here by specializing.
	 */
	StorageT members_;

	template<std::size_t... Is>
	constexpr data(acpp::sycl::queue& q, std::index_sequence<Is...>):
		members_{(static_cast<void>(Is), q)...}
	{}
public:
	data(acpp::sycl::queue& q__):
		data{q__, std::make_index_sequence<sizeof...(Members)>{}}
	{
		q__.wait();
	}

	template<saw::string_literal K>
	auto& get(){
		return std::get<parameter_key_pack_index<K, Members::KeyLiteral...>::value>(members_);
	}
	
	template<saw::string_literal K>
	const auto& get() const {
		return std::get<parameter_key_pack_index<K, Members::KeyLiteral...>::value>(members_);
	}
};

template<typename... Sch, saw::string_literal... Keys, typename Encode>
class data<schema::Ptr<schema::Struct<schema::Member<Sch,Keys>...>>, kel::lbm::encode::Sycl<Encode> > final {
public:
	using StorageT = std::tuple<data<schema::Ptr<Sch>,kel::lbm::encode::Sycl<Encode>>...>;
	using Schema = schema::Struct<schema::Member<schema::Ptr<Sch>,Keys>...>;
private:
	/**
	 * @todo Check by static assert that the members all have the same dimensions. Alternatively
	 * Do it here by specializing.
	 */
	StorageT members_;

public:
	data() = default;

	template<saw::string_literal K>
	auto& get(){
		return std::get<parameter_key_pack_index<K, Keys...>::value>(members_);
	}
	
	template<saw::string_literal K>
	const auto& get() const {
		return std::get<parameter_key_pack_index<K, Keys...>::value>(members_);
	}
};
}

namespace kel {
namespace lbm {
namespace impl {
template<typename Sch, typename Encode>
struct sycl_copy_helper;

template<typename... Members, typename Encode>
struct sycl_copy_helper<sch::Struct<Members...>, Encode> final {
	using Schema = sch::Struct<Members...>;

	template<uint64_t i>
	static saw::error_or<void> copy_to_device_member(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		if constexpr (i < sizeof...(Members)){
			using M = typename saw::parameter_pack_type<i,Members...>::type;
			auto& host_member_data = host_data.template get<M::KeyLiteral>();
			auto& sycl_member_data = sycl_data.template get<M::KeyLiteral>();

			auto host_ptr = host_member_data.flat_data();
			auto sycl_ptr = sycl_member_data.flat_data();

			static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

			q.submit([&](acpp::sycl::handler& h){
				h.copy(host_ptr,sycl_ptr, host_member_data.flat_size().get());
			}).wait();

			return copy_to_device_member<i+1u>(host_data,sycl_data,q);
		}
		
		return saw::make_void();
	}

	static saw::error_or<void> copy_to_device(saw::data<Schema,Encode>& host_data, saw::data<Schema, encode::Sycl<Encode>>& sycl_data, sycl::queue& q){

		return copy_to_device_member<0u>(host_data, sycl_data, q);
	}

	template<uint64_t i>
	static saw::error_or<void> copy_to_host_member(saw::data<Schema,encode::Sycl<Encode>>& sycl_data, saw::data<Schema,Encode>& host_data, sycl::queue& q){
		if constexpr (i < sizeof...(Members)){
			using M = typename saw::parameter_pack_type<i,Members...>::type;
			auto& host_member_data = host_data.template get<M::KeyLiteral>();
			auto& sycl_member_data = sycl_data.template get<M::KeyLiteral>();

			auto host_ptr = host_member_data.flat_data();
			auto sycl_ptr = sycl_member_data.flat_data();

			q.submit([&](acpp::sycl::handler& h){
				h.copy(sycl_ptr,host_ptr, host_member_data.flat_size().get());
			}).wait();

			return copy_to_host_member<i+1u>(sycl_data,host_data,q);
		}
		
		return saw::make_void();
	}
	

	static saw::error_or<void> copy_to_host(saw::data<Schema,Encode>& sycl_data, saw::data<Schema,encode::Sycl<Encode>>& host_data, sycl::queue& q){
		return copy_to_host_member<0u>(sycl_data, host_data, q);
	}
};

template<typename Schema, typename Encode>
struct make_chunk_struct_view_helper;

template<typename... Sch, saw::string_literal... Keys, typename Encode>
struct make_chunk_struct_view_helper<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>> {
private:
	template<uint64_t i>
	static void apply_i(
		saw::data<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::Struct<sch::Member<Sch,Keys>...>>, encode::Sycl<Encode>>& dat_view)
	{
		if constexpr (i < sizeof...(Sch)){
			using M = typename saw::parameter_pack_type<i,sch::Member<Sch,Keys>...>::type;
			
			dat_view.template get<M::KeyLiteral>() = {dat.template get<M::KeyLiteral>().flat_data()};

			apply_i<i+1u>(dat,dat_view);
		}
	}
public:
	static auto apply(saw::data<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>>& dat){
		saw::data<sch::Ptr<sch::Struct<sch::Member<Sch,Keys>...>>, encode::Sycl<Encode>> str_view;

		apply_i<0u>(dat,str_view);

		return str_view;
	}
};
}

// Ptr => Ptr<Chunk<T,Ghost,Dims...>>> => Ptr<FixedArray<T,Dims+Ghost...>
template<typename Schema,typename Encode>
auto make_chunk_struct_view(saw::data<Schema,Encode>& dat){
	return impl::make_chunk_struct_view_helper<Schema,Encode>::apply(dat);
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
	saw::error_or<void> copy_to_device(saw::data<Sch,Encode>& host_data, saw::data<Sch,encode::Sycl<Encode>>& sycl_data){
		return impl::sycl_copy_helper<Sch,Encode>::copy_to_device(host_data, sycl_data, q_);
	}

	template<typename Sch, typename Encode>
	saw::error_or<void> copy_to_host(saw::data<Sch,encode::Sycl<Encode>>& sycl_data, saw::data<Sch,Encode>& host_data){
		return impl::sycl_copy_helper<Sch,Encode>::copy_to_host(sycl_data, host_data, q_);
	}

	auto& get_handle(){
		return q_;
	}
};
}
}
