#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace sch {
namespace impl {
template<typename Sch, uint64_t Ghost, typename LeftG, typename RightG = saw::tmpl_value_group<uint64_t>>
struct chunk_schema_type_helper;

template<typename Sch, uint64_t Ghost, uint64_t Side0, uint64_t... Sides, uint64_t... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, saw::tmpl_value_group<uint64_t,Side0,Sides...>, saw::tmpl_value_group<uint64_t,AddedSides...>> final {
	using Schema = typename chunk_schema_type_helper<Sch,Ghost,saw::tmpl_value_group<uint64_t,Sides...>, saw::tmpl_value_group<uint64_t,AddedSides...,(Side0+2u*(Ghost))>>::Schema;
};

template<typename Sch, uint64_t Ghost, uint64_t... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, saw::tmpl_value_group<uint64_t>, saw::tmpl_value_group<uint64_t,AddedSides...>> final {
	using Schema = FixedArray<Sch,AddedSides...>;
};
}


template<typename Sch, uint64_t Ghost, uint64_t... Sides>
struct Chunk {
	using InnerSchema = typename impl::chunk_schema_type_helper<Sch, Ghost, saw::tmpl_value_group<uint64_t,Sides...>>::Schema;
};

// Not needed for now
template<typename ChunkSchema, uint64_t Dim>
using SuperChunk = Array<ChunkSchema,Dim>;
}
}
}

namespace saw {
template<typename Sch, uint64_t Ghost, uint64_t... Sides, typename Encode>
class data<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>,Encode> final {
public:
	using Schema = kel::lbm::sch::Chunk<Sch,Ghost,Sides...>;
private:
	using InnerSchema = typename Schema::InnerSchema;
	using ValueSchema = typename InnerSchema::ValueType;

	data<InnerSchema, Encode> values_;
public:
	data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	const data<ValueSchema, Encode>& ghost_at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
		return values_.at(index);
	}

	static constexpr auto get_ghost_dims() {
		return data<InnerSchema,Encode>::get_dims();
	}

	data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index){
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return values_.at(ind);
	}

	const data<ValueSchema, Encode>& at(const data<schema::FixedArray<schema::UInt64,sizeof...(Sides)>>& index) const {
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
		return data<InnerSchema,Encode>::flat_dims();
	}
};

template<typename Sch, uint64_t Ghost, uint64_t... Sides>
struct meta_schema<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>> {
	using MetaSchema = typename meta_schema<Sch>::MetaSchema;
};

}
