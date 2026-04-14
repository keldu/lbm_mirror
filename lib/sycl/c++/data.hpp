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
	SAW_FORBID_MOVE(data);
public:
	data(const data<typename meta_schema<Schema>::MetaSchema>& meta__, acpp::sycl::queue& q__):
		q_{&q__},
		values_{nullptr}
	{
		(void) meta__;
		SAW_ASSERT(q_);
		values_ = acpp::sycl::malloc_device<data<Sch,Encode>>(ct_multiply<uint64_t,Dims...>::value,*q_);
		SAW_ASSERT(values_);
	}

	data(acpp::sycl::queue& q__):
		q_{&q__},
		values_{nullptr}
	{
		SAW_ASSERT(q_);
		values_ = acpp::sycl::malloc_device<data<Sch,Encode>>(ct_multiply<uint64_t,Dims...>::value,*q_);
		SAW_ASSERT(values_);
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

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}
	
	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() const {
		return values_;
	}
};

template<typename Sch, uint64_t... Dims, typename Encode>
class data<schema::Ptr<schema::FixedArray<Sch,Dims...>>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ptr<schema::FixedArray<Sch,Dims...>>;
private:
	data<Sch,Encode>* values_;

public:
	SAW_DEFAULT_COPY(data);
	SAW_DEFAULT_MOVE(data);
	
	data():
		values_{nullptr}
	{}

	data(data<schema::FixedArray<Sch,Dims...>, kel::lbm::encode::Sycl<Encode>>& values__):
		values_{values__.flat_data()}
	{}

	data(data<Sch,Encode>* values__):
		values_{values__}
	{}

	static constexpr data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>> get_dims() {
		return saw::data<schema::FixedArray<schema::UInt64, sizeof...(Dims)>>{{Dims...}};
	}

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}
	
	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Dims)>>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,sizeof...(Dims)>::apply(index,get_dims()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() const {
		return values_;
	}
};

template<typename Sch, uint64_t Dims, typename Encode>
class data<schema::Array<Sch,Dims>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::Array<Sch,Dims>;
private:
	static_assert(Dims > 0u, "Zero Dim Arrays make no sense here. If you meant to use this for math style approaches then use Tensor instead of Array");

	data<Sch,Encode>* values_;
	data<schema::FixedArray<schema::UInt64,Dims>,Encode> meta_;
	acpp::sycl::queue* q_;

	SAW_FORBID_COPY(data);
	SAW_FORBID_MOVE(data);
public:
	data(const data<typename meta_schema<Schema>::MetaSchema>& meta__, acpp::sycl::queue& q__):
		q_{&q__},
		values_{nullptr}
	{
		SAW_ASSERT(q_);
		/// TODO use meta
		data<schema::UInt64> m{1u};
		for(uint64_t i = 0u; i < Dims; ++i){
			m = m * meta__.at({i});
		}
		values_ = acpp::sycl::malloc_device<data<Sch,Encode>>(m.get(),*q_);
		SAW_ASSERT(values_);
	}

	data(acpp::sycl::queue& q__):
		values_{nullptr},
		meta_{},
		q_{&q__}
	{
		SAW_ASSERT(q_);
	}

	data(const data<schema::FixedArray<schema::UInt64,Dims>, Encode>& meta__,acpp::sycl::queue& q__):
		values_{nullptr},
		meta_{meta__},
		q_{&q__}
	{
		SAW_ASSERT(q_);
		values_ = acpp::sycl::malloc_device<data<Sch,Encode>>(flat_size().get(),*q_);
		SAW_ASSERT(values_);
	}

	~data(){
		if(not values_){
			return;
		}
		SAW_ASSERT(q_);

		acpp::sycl::free(values_,*q_);
		values_ = nullptr;
	}

	constexpr data<schema::FixedArray<schema::UInt64, Dims>, Encode> meta() const {
		return meta_;
	}

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,Dims>, Encode>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,Dims>::apply(index,meta()).get()];
	}

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,Dims>, Encode>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,Dims>::apply(index,meta()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() const {
		return values_;
	}

	constexpr data<schema::UInt64,Encode> flat_size() const {
		data<schema::UInt64> mult{1u};

		for(uint64_t i{0u}; i < Dims; ++i){
			mult = mult * meta_.at({i});
		}

		return mult;
	}
};

template<typename Sch, uint64_t Dims, typename Encode>
class data<schema::Ptr<schema::Array<Sch,Dims>>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ptr<schema::Array<Sch,Dims>>;
private:
	static_assert(Dims > 0u, "Zero Dim Arrays make no sense here. If you meant to use this for math style approaches then use Tensor instead of Array");

	data<Sch,Encode>* values_;
	data<schema::FixedArray<schema::UInt64,Dims>,Encode> meta_;

public:
	SAW_DEFAULT_COPY(data);
	SAW_DEFAULT_MOVE(data);

	data():
		values_{nullptr}
	{}

	data(const data<schema::Array<Sch,Dims>, kel::lbm::encode::Sycl<Encode>>& values__):
		values_{values__.flat_data()},
		meta_{values__.meta()}
	{}

	constexpr data<schema::FixedArray<schema::UInt64, Dims>, Encode> meta() const {
		return meta_;
	}

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,Dims>, Encode>& index){
		return values_[kel::lbm::flatten_index<schema::UInt64,Dims>::apply(index,meta()).get()];
	}

	constexpr data<Sch,Encode>& at(const data<schema::FixedArray<schema::UInt64,Dims>, Encode>& index) const{
		return values_[kel::lbm::flatten_index<schema::UInt64,Dims>::apply(index,meta()).get()];
	}

	constexpr data<Sch,Encode>* flat_data() const {
		return values_;
	}

	constexpr data<schema::UInt64,Encode> flat_size() const {
		data<schema::UInt64> mult{1u};

		for(uint64_t i{0u}; i < Dims; ++i){
			mult = mult * meta_.at({i});
		}

		return mult;
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
	data(const data<typename meta_schema<Schema>::MetaSchema>& meta__, acpp::sycl::queue& q__):
		values_{meta__,q__}
	{}

	data(acpp::sycl::queue& q__):
		values_{q__}
	{}

	constexpr data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	constexpr data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
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
		return data<schema::FixedArray<schema::UInt64, sizeof...(Sides)>,Encode>{{Sides...}};
	}

	auto flat_data() const {
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

public:
	SAW_DEFAULT_MOVE(data);
	SAW_DEFAULT_COPY(data);

	data():
		values_{nullptr}
	{}

	data(const data<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>, kel::lbm::encode::Sycl<Encode>>& values__):
		values_{values__.flat_data()}
	{}

	data(data<Sch,Encode>* values__):
		values_{values__}
	{}

	constexpr data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	constexpr data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
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
		return data<schema::FixedArray<schema::UInt64, sizeof...(Sides)>,Encode>{{Sides...}};
	}

	auto flat_data(){
		return values_.flat_data();
	}

	static constexpr auto flat_size() {
		return data<InnerSchema,kel::lbm::encode::Sycl<Encode>>::flat_size();
	}
};

template<typename... Members, typename Encode>
struct data<schema::Tuple<Members...>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using StorageT = std::tuple<data<Members,kel::lbm::encode::Sycl<Encode>>...>;
	using Schema = schema::Tuple<Members...>;
private:
	StorageT members_;

	/**
	 * A helper constructor to forward the sycl queue to the inner "default" constructors
	 */
	template<std::size_t... Is>
	constexpr data(acpp::sycl::queue& q, std::index_sequence<Is...>):
		members_{(static_cast<void>(Is), q)...}
	{}
public:
	data(data<typename meta_schema<Schema>::MetaSchema>& meta__, acpp::sycl::queue& q__):
		data{q__, std::make_index_sequence<sizeof...(Members)>{}}
	{
	}

	data(acpp::sycl::queue& q__):
		data{q__, std::make_index_sequence<sizeof...(Members)>{}}
	{
		q__.wait();
	}

	template<size_t i>
	auto& get(){
		return std::get<i>(members_);
	}

	template<size_t i>
	auto& get() const {
		return std::get<i>(members_);
	}
};

template<typename... Members, typename Encode>
struct data<schema::Ptr<schema::Tuple<Members...>>, kel::lbm::encode::Sycl<Encode>> final {
public:
	using StorageT = std::tuple<data<schema::Ptr<Members>,kel::lbm::encode::Sycl<Encode>>...>;
	using Schema = schema::Tuple<Members...>;
private:
	StorageT members_;

	/**
	 * A helper constructor to forward the sycl queue to the inner "default" constructors
	 */
public:
	data() = default;

	template<size_t i>
	auto& get(){
		return std::get<i>(members_);
	}

	template<size_t i>
	const auto& get() const {
		return std::get<i>(members_);
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

	/**
	 * A helper constructor to forward the sycl queue to the inner "default" constructors
	 */
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

	template<size_t i>
	auto& get(){
		return std::get<i>(members_);
	}

	template<size_t i>
	auto& get() const {
		return std::get<i>(members_);
	}

	template<saw::string_literal K>
	auto& get(){
		return std::get<parameter_key_pack_index<K, Members::KeyLiteral...>::value>(members_);
	}
	
	template<saw::string_literal K>
	auto& get() const {
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

	template<size_t i>
	auto& get(){
		return std::get<i>(members_);
	}

	template<size_t i>
	auto& get() const {
		return std::get<i>(members_);
	}

	template<saw::string_literal K>
	auto& get(){
		return std::get<parameter_key_pack_index<K, Keys...>::value>(members_);
	}
	
	template<saw::string_literal K>
	auto& get() const {
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

			auto eov = sycl_copy_helper<typename M::ValueType,Encode>::copy_to_device(host_member_data,sycl_member_data,q);
			if(eov.is_error()){
				return eov;
			}

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

			auto eov = sycl_copy_helper<typename M::ValueType,Encode>::copy_to_host(sycl_member_data,host_member_data,q);
			if(eov.is_error()){
				return eov;
			}

			return copy_to_host_member<i+1u>(sycl_data,host_data,q);
		}
		
		return saw::make_void();
	}
	

	static saw::error_or<void> copy_to_host(saw::data<Schema,encode::Sycl<Encode>>& sycl_data, saw::data<Schema,Encode>& host_data, sycl::queue& q){
		return copy_to_host_member<0u>(sycl_data, host_data, q);
	}

	template<uint64_t i>
	static saw::error_or<void> malloc_on_device_member(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){

		if constexpr (i < sizeof...(Members)){
			using M = typename saw::parameter_pack_type<i,Members...>::type;
			auto& host_member_data = host_data.template get<i>();
			auto& sycl_member_data = sycl_data.template get<i>();

			auto eov = sycl_copy_helper<typename M::ValueType,Encode>::malloc_on_device(host_member_data,sycl_member_data,q);
			if(eov.is_error()){
				return eov;
			}

			return malloc_on_device_member<i+1u>(host_data,sycl_data,q);
		}

		return saw::make_void();
	}

	static saw::error_or<void> malloc_on_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		return malloc_on_device_member<0u>(host_data,sycl_data,q);
	}
};

template<typename... Members, typename Encode>
struct sycl_copy_helper<sch::Tuple<Members...>, Encode> final {
	using Schema = sch::Tuple<Members...>;

	template<uint64_t i>
	static saw::error_or<void> copy_to_device_member(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		if constexpr (i < sizeof...(Members)){
			using M = typename saw::parameter_pack_type<i,Members...>::type;
			auto& host_member_data = host_data.template get<i>();
			auto& sycl_member_data = sycl_data.template get<i>();

			auto eov = sycl_copy_helper<M,Encode>::copy_to_device(host_member_data,sycl_member_data,q);
			if(eov.is_error()){
				return eov;
			}

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

			auto& host_member_data = host_data.template get<i>();
			auto& sycl_member_data = sycl_data.template get<i>();

			auto eov = sycl_copy_helper<M,Encode>::copy_to_host(sycl_member_data,host_member_data,q);
			if(eov.is_error()){
				return eov;
			}

			return copy_to_host_member<i+1u>(sycl_data,host_data,q);
		}

		return saw::make_void();
	}


	static saw::error_or<void> copy_to_host(
		saw::data<Schema,Encode>& host_data,
		saw::data<Schema,encode::Sycl<Encode>>& sycl_data,
		sycl::queue& q
	){
		return copy_to_host_member<0u>(sycl_data, host_data, q);
	}

	template<uint64_t i>
	static saw::error_or<void> malloc_on_device_member(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){

		if constexpr (i < sizeof...(Members)){
			using M = typename saw::parameter_pack_type<i,Members...>::type;
			auto& host_member_data = host_data.template get<i>();
			auto& sycl_member_data = sycl_data.template get<i>();

			auto eov = sycl_copy_helper<M,Encode>::malloc_on_device(host_member_data,sycl_member_data,q);
			if(eov.is_error()){
				return eov;
			}

			return malloc_on_device_member<i+1u>(host_data,sycl_data,q);
		}

		return saw::make_void();
	}

	static saw::error_or<void> malloc_on_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		return malloc_on_device_member<0u>(host_data,sycl_data,q);
	}
};

template<typename Sch, uint64_t... Dims, typename Encode>
struct sycl_copy_helper<sch::FixedArray<Sch,Dims...>, Encode> final {
	using Schema = sch::FixedArray<Sch,Dims...>;
	
	static saw::error_or<void> copy_to_host(saw::data<Schema,encode::Sycl<Encode>>& sycl_data, saw::data<Schema,Encode>& host_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(sycl_ptr,host_ptr, saw::ct_multiply<uint64_t,Dims...>::value);
		}).wait();
		return saw::make_void();
	}

	static saw::error_or<void> copy_to_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(host_ptr,sycl_ptr, saw::ct_multiply<uint64_t,Dims...>::value);
		}).wait();
		return saw::make_void();
	}

	static saw::error_or<void> malloc_on_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		(void) host_data;
		(void) sycl_data;
		(void) q;
		return saw::make_void();
	}
};

template<typename Sch, uint64_t Ghost, uint64_t... Dims, typename Encode>
struct sycl_copy_helper<sch::Chunk<Sch,Ghost,Dims...>, Encode> final {
	using Schema = sch::Chunk<Sch,Ghost,Dims...>;

	static saw::error_or<void> copy_to_host(saw::data<Schema,encode::Sycl<Encode>>& sycl_data, saw::data<Schema,Encode>& host_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(sycl_ptr,host_ptr, saw::ct_multiply<uint64_t,Dims...>::value);
		}).wait();
		return saw::make_void();
	}

	static saw::error_or<void> copy_to_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(host_ptr,sycl_ptr, saw::ct_multiply<uint64_t,Dims...>::value);
		}).wait();
		return saw::make_void();
	}
};

template<typename Sch, uint64_t Dims, typename Encode>
struct sycl_copy_helper<sch::Array<Sch,Dims>, Encode> final {
	using Schema = sch::Array<Sch,Dims>;

	static saw::error_or<void> copy_to_host(saw::data<Schema,encode::Sycl<Encode>>& sycl_data, saw::data<Schema,Encode>& host_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(sycl_ptr,host_ptr, host_data.flat_size().get());
		}).wait();
		return saw::make_void();
	}

	static saw::error_or<void> copy_to_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		auto host_ptr = host_data.flat_data();
		auto sycl_ptr = sycl_data.flat_data();

		static_assert(sizeof(std::decay_t<decltype(sycl_ptr)>) == sizeof(std::decay_t<decltype(host_ptr)>), "Unequal size");

		q.submit([&](acpp::sycl::handler& h){
			h.copy(host_ptr,sycl_ptr, host_data.flat_size().get());
		}).wait();
		return saw::make_void();
	}

	static saw::error_or<void> malloc_on_device(saw::data<Schema,Encode>& host_data, saw::data<Schema,encode::Sycl<Encode>>& sycl_data, sycl::queue& q){
		sycl_data = {host_data.meta(),q};
		return saw::make_void();
	}
};


template<typename Schema, typename Encode>
struct make_view_helper;

template<typename Sch, uint64_t Dims, typename Encode>
struct make_view_helper<sch::Array<Sch,Dims>, encode::Sycl<Encode>> {
public:
static saw::error_or<void> apply(
	saw::data<sch::Array<Sch,Dims>, encode::Sycl<Encode>>& dat,
	saw::data<sch::Ptr<sch::Array<Sch,Dims>>, encode::Sycl<Encode>>& dat_view
){
	dat_view = {dat};
	return saw::make_void();
}
};

template<typename Sch, uint64_t... Dims, typename Encode>
struct make_view_helper<sch::FixedArray<Sch,Dims...>, encode::Sycl<Encode>> {
public:
	static saw::error_or<void> apply(
		saw::data<sch::FixedArray<Sch,Dims...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::FixedArray<Sch,Dims...>>, encode::Sycl<Encode>>& dat_view
	){
		dat_view = {dat};
		return saw::make_void();
	}
};

template<typename Sch, uint64_t Ghost, uint64_t... Dims, typename Encode>
struct make_view_helper<sch::Chunk<Sch,Ghost,Dims...>, encode::Sycl<Encode>> {
public:
	static saw::error_or<void> apply(
		saw::data<sch::Chunk<Sch,Ghost,Dims...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::Chunk<Sch,Ghost,Dims...>>, encode::Sycl<Encode>>& dat_view
	){
		dat_view = {dat};
		return saw::make_void();
	}
};

template<typename... Sch, saw::string_literal... Keys, typename Encode>
struct make_view_helper<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>> {
private:
template<uint64_t i>
static saw::error_or<void> apply_i(
		saw::data<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::Struct<sch::Member<Sch,Keys>...>>, encode::Sycl<Encode>>& dat_view
	){
		if constexpr (i < sizeof...(Sch)){
			using M = typename saw::parameter_pack_type<i,sch::Member<Sch,Keys>...>::type;

			auto eov = make_view_helper<typename M::ValueType,encode::Sycl<Encode>>::apply(dat.template get<M::KeyLiteral>(),dat_view.template get<M::KeyLiteral>());
			if(eov.is_error()){
				return eov;
			}

			return apply_i<i+1u>(dat,dat_view);
		}

		return saw::make_void();
	}
public:
static saw::error_or<void> apply(
	saw::data<sch::Struct<sch::Member<Sch,Keys>...>, encode::Sycl<Encode>>& dat,
	saw::data<sch::Ptr<sch::Struct<sch::Member<Sch,Keys>...>>, encode::Sycl<Encode>>& dat_view
){
	return apply_i<0u>(dat,dat_view);
}
};

template<typename... Sch, typename Encode>
struct make_view_helper<sch::Tuple<Sch...>, encode::Sycl<Encode>> {
private:
	template<uint64_t i>
	static saw::error_or<void> apply_i(
		saw::data<sch::Tuple<Sch...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::Tuple<Sch...>>, encode::Sycl<Encode>>& dat_view
	){
		if constexpr (i < sizeof...(Sch)){
			using M = typename saw::parameter_pack_type<i,Sch...>::type;

			auto eov = make_view_helper<M,encode::Sycl<Encode>>::apply(dat.template get<i>(), dat_view.template get<i>());
			if(eov.is_error()){
				return eov;
			}

			return apply_i<i+1u>(dat,dat_view);
		}

		return saw::make_void();
	}
public:
	static saw::error_or<void> apply(
		saw::data<sch::Tuple<Sch...>, encode::Sycl<Encode>>& dat,
		saw::data<sch::Ptr<sch::Tuple<Sch...>>, encode::Sycl<Encode>> dat_view
	){
		return apply_i<0u>(dat,dat_view);
	}
};
}

// Ptr => Ptr<Chunk<T,Ghost,Dims...>>> => Ptr<FixedArray<T,Dims+Ghost...>
template<typename Schema,typename Encode>
auto make_view(saw::data<Schema,Encode>& dat){
	saw::data<sch::Ptr<Schema>,Encode> dat_view;
	auto eov = impl::make_view_helper<Schema,Encode>::apply(dat,dat_view);
	(void) eov;

	return dat_view;
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

	template<typename Sch, typename Encode>
	saw::error_or<void> malloc_on_device(
			saw::data<Sch,Encode>& host_data,
			saw::data<Sch,encode::Sycl<Encode>>& sycl_data
			){
		auto eov = impl::sycl_copy_helper<Sch,Encode>::malloc_on_device(host_data, sycl_data, q_);
		q_.wait();
		return eov;
	}

	auto& get_handle(){
		return q_;
	}
};
}
}
