#pragma once

#include "abstract/data.hpp"
#include "abstract/data_math.hpp"
#include "abstract/schema_factory.hpp"

#include "descriptor/d1q3.hpp"
#include "descriptor/d2q5.hpp"
#include "descriptor/d2q9.hpp"
#include "descriptor/d3q19.hpp"
#include "descriptor/d3q27.hpp"

namespace kel {
namespace lbm {
namespace sch {

template<typename Sch, typename Desc, uint64_t SC_V, uint64_t DC_V, uint64_t QC_V>
struct Cell {
	/// Mirrors the meta_schema specialisation further down this file.
	using Meta = Void;
	using Descriptor = Desc;
	static constexpr uint64_t SC = SC_V;
	static constexpr uint64_t DC = DC_V;
	static constexpr uint64_t QC = QC_V;
	static constexpr uint64_t Size = SC + Desc::D * DC + Desc::Q * QC;
};

template<typename Desc, typename Cell>
struct CellField{
	/// A field is laid out over the grid, so the grid extents are its metadata.
	using Meta = FixedArray<UInt64, Desc::D>;
	using Descriptor = Desc;
};

template<typename Desc, typename... CellFieldTypes, kel::lbm::string_literal... CellFieldNames>
struct CellField<
	Desc,
	Struct<
		Member<CellFieldTypes, CellFieldNames>...
	>
> {
	using Meta = FixedArray<UInt64, Desc::D>;
	using Descriptor = Desc;
};

template<typename Desc, typename... CellFieldMembers>
struct CellFieldStruct {
	using Meta = FixedArray<UInt64, Desc::D>;
	using Descriptor = Desc;
};

}

/*
namespace impl {
template<typename Desc>
struct df_ct_helper {
	template<uint64_t i>
	static constexpr uint64_t apply_i(const std::array<std::array<int32_t,Desc::D>,Desc::Q>& dirs, const std::array<T,Desc::D+1u>& inp){
		if constexpr ( i < Desc::Q ){
			for(uint64_t j = 0u; j < Desc::D; ++j){
			}
		}
		return 0u;
	}
};
}
*/

template<typename Schema>
class cell_schema_builder {
private:
	kel::lbm::schema_factory<Schema> factory_struct_;
public:
	cell_schema_builder() = default;

	cell_schema_builder(kel::lbm::schema_factory<Schema> inp):
		factory_struct_{inp}
	{}

	/*
	template<typename TA, kel::lbm::string_literal KA>
	constexpr auto require() const noexcept {
		return {factory_struct_.add_maybe()};
	}
	*/
};
 
}
}

namespace kel {
namespace lbm {
template<typename T, typename Desc, uint64_t S, uint64_t D, uint64_t Q>
struct meta_schema<kel::lbm::sch::Cell<T,Desc,S,D,Q>> {
	using MetaSchema = sch::Void;
	using Schema = kel::lbm::sch::Cell<T,Desc,S,D,Q>;
};

template<typename Desc, typename CellT>
struct meta_schema<kel::lbm::sch::CellField<Desc, CellT>> {
	using MetaSchema = sch::FixedArray<sch::UInt64,Desc::D>;
	using Schema = kel::lbm::sch::CellField<Desc, CellT>;
};

template<typename Desc, typename... CellFieldsT>
struct meta_schema<kel::lbm::sch::CellFieldStruct<Desc,sch::Struct<CellFieldsT...>>> {
	using MetaSchema = sch::FixedArray<sch::UInt64,Desc::D>;
	using Schema = kel::lbm::sch::CellFieldStruct<Desc,sch::Struct<CellFieldsT...>>;
};

template<typename Sch, typename Desc, uint64_t S, uint64_t D, uint64_t Q, typename Encode>
class data<kel::lbm::sch::Cell<Sch, Desc, S, D, Q>, Encode> final {
public:
	using Schema = kel::lbm::sch::Cell<Sch,Desc,S,D,Q>;
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	data<sch::FixedArray<Sch, Schema::Size>, Encode> inner_;
public:
	data() = default;

	data<Sch, Encode>& operator()(const data<sch::UInt64>& index){
		return inner_.at(index);
	}

	const data<Sch, Encode>& operator()(const data<sch::UInt64>& index)const{
		return inner_.at(index);
	}

	const data<kel::lbm::sch::Cell<Sch, Desc, S, D, Q>, Encode> copy() const {
		return *this;
	}
};

/*
 * Create a cellfield
 */
template<typename Desc, typename CellT, typename Encode>
class data<kel::lbm::sch::CellField<Desc, CellT>, Encode> final {
public:
	using Schema = kel::lbm::sch::CellField<Desc,CellT>;
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	data<sch::Array<CellT,Desc::D>, Encode> inner_;
public:
	data() = default;
	data(const data<MetaSchema,Encode>& inner_meta__):
		inner_{inner_meta__}
	{}

	const data<MetaSchema, Encode> meta() const {
		return inner_.dims();
	}

	template<uint64_t i>
	data<sch::UInt64,Encode> get_dim_size() const {
		static_assert(i < Desc::D, "Not enough dimensions");
		return inner_.template get_dim_size<i>();
	}

	const data<CellT>& operator()(const data<sch::FixedArray<sch::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& operator()(const data<sch::FixedArray<sch::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	const data<CellT>& at(const data<sch::FixedArray<sch::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& at(const data<sch::FixedArray<sch::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	data<sch::UInt64,Encode> internal_size() const {
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
class data<kel::lbm::sch::CellFieldStruct<Desc, sch::Struct<CellFieldsT...>>, Encode> final {
public:
	using Schema = kel::lbm::sch::CellFieldStruct<Desc,sch::Struct<CellFieldsT...>>;
	/// @TODO Add MetaSchema to Schema
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	static_assert(sizeof...(CellFieldsT) > 0u, "");
	data<sch::Struct<CellFieldsT...>, Encode> inner_;

	template<uint64_t i>
	kel::lbm::error_or<void> helper_constructor(const data<sch::FixedArray<sch::UInt64,Desc::D>>& grid_size){
		using MemT = kel::lbm::parameter_pack_type<i,CellFieldsT...>::type;
		{
			inner_.template get<MemT::KeyLiteral>() = {grid_size};
		}
		if constexpr (sizeof...(CellFieldsT) > (i+1u)){
			return helper_constructor<i+1u>(grid_size);
		}
		return kel::lbm::make_void();
	}
public:
	data() = delete;

	data(const data<sch::FixedArray<sch::UInt64,Desc::D>>& grid_size__){
		auto eov = helper_constructor<0u>(grid_size__);
		(void)eov;
	}

	const data<MetaSchema, Encode> meta() const {
		using MemT = kel::lbm::parameter_pack_type<0u,CellFieldsT...>::type;
		return inner_.template get<MemT::KeyLiteral>().meta();
	}

	template<kel::lbm::string_literal Key>
	auto& get() {
		return inner_.template get<Key>();
	}

	template<kel::lbm::string_literal Key>
	const auto& get() const {
		return inner_.template get<Key>();
	}
};


}
}
