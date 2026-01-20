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
template<typename Sch, uint64_t Dim, uint64_t Side, uint64_t Ghost, typename Encode>
class data<kel::lbm::sch::Chunk<Sch,Dim,Side,Ghost>,Encode> final {
public:
	using Schema = kel::lbm::sch::Chunk<Sch,Dim,Side,Ghost>;
private:
	data<typename Schema::InnerSchema, Encode> values_;
public:
};
}
