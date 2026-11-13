#pragma once

#include "common.hpp"
#include "flatten.hpp"

namespace kel {
namespace lbm {
namespace sch {
namespace impl {
template<typename Sch, uint64_t Ghost, typename LeftG, typename RightG = kel::lbm::tmpl_value_group<uint64_t>>
struct chunk_schema_type_helper;

template<typename Sch, uint64_t Ghost, uint64_t Side0, uint64_t... Sides, uint64_t... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, kel::lbm::tmpl_value_group<uint64_t,Side0,Sides...>, kel::lbm::tmpl_value_group<uint64_t,AddedSides...>> final {
	using Schema = typename chunk_schema_type_helper<Sch,Ghost,kel::lbm::tmpl_value_group<uint64_t,Sides...>, kel::lbm::tmpl_value_group<uint64_t,AddedSides...,(Side0+2u*(Ghost))>>::Schema;
};

template<typename Sch, uint64_t Ghost, uint64_t... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, kel::lbm::tmpl_value_group<uint64_t>, kel::lbm::tmpl_value_group<uint64_t,AddedSides...>> final {
	using Schema = FixedArray<Sch,AddedSides...>;
};
}


template<typename Sch, uint64_t Ghost, uint64_t... Sides>
struct Chunk {
	/**
	 * A chunk's extents are compile time constants, so it carries nothing of
	 * its own at runtime; whatever metadata its stored value needs is all
	 * there is.
	 */
	using Meta = typename Sch::Meta;
	using InnerSchema = typename impl::chunk_schema_type_helper<Sch, Ghost, kel::lbm::tmpl_value_group<uint64_t,Sides...>>::Schema;
	using StoredValueSchema = Sch;
};

// Not needed for now
template<typename ChunkSchema, uint64_t Dim>
using SuperChunk = Array<ChunkSchema,Dim>;
}
}
}

namespace kel {
namespace lbm {

template<typename Sch, uint64_t Ghost, uint64_t... Sides, typename Encode>
class data<kel::lbm::sch::Chunk<Sch,Ghost,Sides...>,Encode> final {
public:
	using Schema = kel::lbm::sch::Chunk<Sch,Ghost,Sides...>;
private:
	using InnerSchema = typename Schema::InnerSchema;
	using ValueSchema = typename InnerSchema::ValueType;

	data<InnerSchema, Encode> values_;
public:
	data<ValueSchema, Encode>& ghost_at(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index){
		return values_.at(index);
	}

	const data<ValueSchema, Encode>& ghost_at(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index) const {
		return values_.at(index);
	}

	static constexpr auto get_ghost_dims() {
		return data<InnerSchema,Encode>::get_dims();
	}

	static constexpr auto to_ghost_index(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index){
		std::decay_t<decltype(index)> ind;
		for(uint64_t i = 0u; i < sizeof...(Sides); ++i){
			ind.at({i}) = index.at({i}) + Ghost;
		}
		return ind;
	}

	data<ValueSchema, Encode>& at(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index){
		std::decay_t<decltype(index)> ind = to_ghost_index(index);
		return values_.at(ind);
	}

	const data<ValueSchema, Encode>& at(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index) const {
		std::decay_t<decltype(index)> ind = to_ghost_index(index);
		return values_.at(ind);
	}

	/*
	const data<ValueSchema, Encode>& neighbour_at(const data<sch::FixedArray<sch::UInt64,sizeof...(Sides)>>& index) const {
	}
	*/

	static constexpr auto get_dims(){
		return data<sch::FixedArray<sch::UInt64, sizeof...(Sides)>,Encode>{{Sides...}};
	}

	auto flat_data(){
		return values_.flat_data();
	}

	/**
	 * The const counterpart data<FixedArray> has always had. Without it a
	 * const-correct reader -- a Forst encode, or a writer taking
	 * `const data<Sch>&` the way write_vtk_file does -- cannot reach the
	 * bytes at all.
	 */
	auto flat_data() const {
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
}
