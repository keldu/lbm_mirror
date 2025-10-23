#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/data_math.hpp>
#include <forstio/codec/schema_factory.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<uint64_t DV, uint64_t QV>
struct Descriptor {
	static constexpr uint64_t D = DV;
	static constexpr uint64_t Q = QV;
};

template<typename Sch, typename Desc, uint64_t SC_V, uint64_t DC_V, uint64_t QC_V>
struct Cell {
	using Descriptor = Desc;
	static constexpr uint64_t SC = SC_V;
	static constexpr uint64_t DC = DC_V;
	static constexpr uint64_t QC = QC_V;
	static constexpr uint64_t Size = SC + Desc::D * DC + Desc::Q * QC;
};

template<typename Desc, typename Cell>
struct CellField{
	using Descriptor = Desc;
};

template<typename Desc, typename... CellFieldTypes, saw::string_literal... CellFieldNames>
struct CellField<
	Desc,
	Struct<
		Member<CellFieldTypes, CellFieldNames>...
	>
> {
	using Descriptor = Desc;
	using Schema = CellField<Desc,Struct<Member<CellFieldTypes,CellFieldNames>...>>;
	using MetaSchema = FixedArray<UInt64,Desc::D>;
};

template<typename Desc, typename... CellFieldMembers>
struct CellFieldStruct {
	using Schema = CellFieldStruct<Desc, Struct<CellFieldMembers...>>;
	using InnerSchema = Struct<CellFieldMembers...>;
	using MetaSchema = typename InnerSchema::MetaSchema;
	// using MetaSchema = FixedArray<UInt64, Desc::D>;
};

}

template<typename T, typename Desc>
class df_info{};

template<typename T> 
class df_info<T,sch::Descriptor<1,3>> {
public:
	using Descriptor = sch::Descriptor<1,3>;

	static constexpr uint64_t D = 1u;
	static constexpr uint64_t Q = 3u;

	static constexpr std::array<std::array<int32_t,D>,Q> directions = {{
		{ 0},
		{-1},
		{ 1}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type, Q> weights = {
		2./3.,
		1./6.,
		1./6.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,2,1
	};
	
	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};

/**
 * D2Q5 Descriptor
 */
template<typename T>
class df_info<T,sch::Descriptor<2, 5>> {
public:
	using Descriptor = sch::Descriptor<2,5>;

	static constexpr uint64_t D = 2u;
	static constexpr uint64_t Q = 5u;

	static constexpr std::array<std::array<int32_t, D>, Q> directions = {{
		{ 0, 0},
		{-1, 0},
		{ 1, 0},
		{ 0,-1},
		{ 0, 1},
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
		1./3.,
		1./6.,
		1./6.,
		1./6.,
		1./6.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,
		2,
		1,
		4,
		3
	};

	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};

template<typename T>
class df_info<T,sch::Descriptor<2, 9>> {
public:
	using Descriptor = sch::Descriptor<2,9>;
	
	static constexpr uint64_t D = 2u;
	static constexpr uint64_t Q = 9u;

	static constexpr std::array<std::array<int32_t, D>, Q> directions = {{
		{ 0, 0},
		{-1, 0},
		{ 1, 0},
		{ 0,-1},
		{ 0, 1},
		{-1,-1},
		{ 1, 1},
		{-1, 1},
		{ 1,-1}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
		4./9.,
		1./9.,
		1./9.,
		1./9.,
		1./9.,
		1./36.,
		1./36.,
		1./36.,
		1./36.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,
		2,
		1,
		4,
		3,
		6,
		5,
		8,
		7
	};
	
	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
/*
template<typename T>
class df_info<T,sch::Descriptor<3, 27>> {
public:
	using Descriptor = sch::Descriptor<3,27>;

	static constexpr uint64_t D = 3u;
	static constexpr uint64_t Q = 27u;

	static constexpr std::array<std::array<int32_t, D>, Q> directions = {{
		{ 0, 0, 0},
		{-1, 0, 0},
		{ 1, 0, 0},
		{ 0,-1, 0},
		{ 0, 1, 0},
		{-1,-1, 0},
		{ 1, 1, 0},
		{-1, 1, 0},
		{ 1,-1, 0}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
		4./9.,
		1./9.,
		1./9.,
		1./9.,
		1./9.,
		1./36.,
		1./36.,
		1./36.,
		1./36.
	};

	static constexpr std::array<uint64_t,Q> opposite_index = {
		0,
		2,
		1,
		4,
		3,
		6,
		5,
		8,
		7
	};

	static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
*/

template<typename Schema>
class cell_schema_builder {
private:
	saw::schema_factory<Schema> factory_struct_;
public:
	cell_schema_builder() = default;

	cell_schema_builder(saw::schema_factory<Schema> inp):
		factory_struct_{inp}
	{}

	/*
	template<typename TA, saw::string_literal KA>
	constexpr auto require() const noexcept {
		return {factory_struct_.add_maybe()};
	}
	*/
};
 
}
}

namespace saw {
template<typename T, typename Desc, uint64_t S, uint64_t D, uint64_t Q>
struct meta_schema<kel::lbm::sch::Cell<T,Desc,S,D,Q>> {
  using MetaSchema = schema::Void;
	using Schema = kel::lbm::sch::Cell<T,Desc,S,D,Q>;
};

template<typename Desc, typename CellT>
struct meta_schema<kel::lbm::sch::CellField<Desc, CellT>> {
  using MetaSchema = schema::FixedArray<schema::UInt64,Desc::D>;
	using Schema = kel::lbm::sch::CellField<Desc, CellT>;
};

template<typename Sch, typename Desc, uint64_t S, uint64_t D, uint64_t Q, typename Encode>
class data<kel::lbm::sch::Cell<Sch, Desc, S, D, Q>, Encode> final {
public:
	using Schema = kel::lbm::sch::Cell<Sch,Desc,S,D,Q>;
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	data<schema::FixedArray<Sch, Schema::Size>, Encode> inner_;
public:
	data() = default;

	data<Sch, Encode>& operator()(const data<schema::UInt64>& index){
		return inner_.at(index);
	}

	const data<Sch, Encode>& operator()(const data<schema::UInt64>& index)const{
		return inner_.at(index);
	}

	const data<kel::lbm::sch::Cell<Sch, Desc, S, D, Q>, Encode> copy() const {
		return *this;
	}
};

template<typename Desc, typename CellT, typename Encode>
class data<kel::lbm::sch::CellField<Desc, CellT>, Encode> final {
public:
	using Schema = kel::lbm::sch::CellField<Desc,CellT>;
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	data<schema::Array<CellT,Desc::D>, Encode> inner_;
public:
	data() = default;
	data(const data<MetaSchema,Encode>& inner_meta__):
		inner_{inner_meta__}
	{}

	const data<MetaSchema, Encode> meta() const {
		return inner_.dims();
	}

	template<uint64_t i>
	data<schema::UInt64,Encode> get_dim_size() const {
		static_assert(i < Desc::D, "Not enough dimensions");
		return inner_.template get_dim_size<i>();
	}

	const data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	const data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	data<schema::UInt64,Encode> internal_size() const {
		return inner_.internal_size();
	}

	data<CellT,Encode>* internal_data() {
		return inner_.internal_data();
	}
};

/**
 * Is basically a struct, but additionally has members for meta() calls. It technically is a Field of a struct, but organized through a struct of fields.
 */
template<typename Desc, typename... CellFieldsT, typename Encode>
class data<kel::lbm::sch::CellFieldStruct<Desc, schema::Struct<CellFieldsT...>>, Encode> final {
public:
	using Schema = kel::lbm::sch::CellFieldStruct<Desc,schema::Struct<CellFieldsT...>>;
	/// @TODO Add MetaSchema to Schema
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	static_assert(sizeof...(CellFieldsT) > 0u, "");
	data<schema::Struct<CellFieldsT...>, Encode> inner_;

	template<uint64_t i>
	saw::error_or<void> helper_constructor(const data<schema::FixedArray<schema::UInt64,Desc::D>>& grid_size){
		using MemT = saw::parameter_pack_type<i,CellFieldsT...>::type;
		{
			inner_.template get<MemT::KeyLiteral>() = {grid_size};
		}
		if constexpr (sizeof...(CellFieldsT) > (i+1u)){
			return helper_constructor<i+1u>(grid_size);
		}
		return saw::make_void();
	}
public:
	data() = delete;

	data(const data<schema::FixedArray<schema::UInt64,Desc::D>>& grid_size__){
		auto eov = helper_constructor<0u>(grid_size__);
		(void)eov;
	}

	const data<MetaSchema, Encode> meta() const {
		using MemT = saw::parameter_pack_type<0u,CellFieldsT...>::type;
		return inner_.template get<MemT::KeyLiteral>().meta();
	}

	template<saw::string_literal Key>
	auto& get() {
		return inner_.template get<Key>();
	}

	template<saw::string_literal Key>
	const auto& get() const {
		return inner_.template get<Key>();
	}
};


}
