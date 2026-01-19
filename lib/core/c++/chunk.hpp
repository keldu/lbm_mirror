#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace sch {
namespace impl {
template<typename Sch, uint64_t Ghost, typename LeftG, typename RightG>
struct chunk_schema_type_helper {
	using Schema = typename chunk_schema_type_helper<Sch,Ghost,LeftG,RightG>::Schema;
};

template<typename Sch, uint64_t Ghost, typename Side0, typename... Sides, typename... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, saw::tmpl_group<Side0,Sides...>, saw::tmpl_group<AddedSides...>> final {
	using Schema = FixedArray<Sch,AddedSides...>;
};

template<typename Sch, uint64_t Ghost, uint64_t... AddedSides>
struct chunk_schema_type_helper<Sch, Ghost, saw::tmpl_group<Side0,Sides...>, saw::tmpl_group<AddedSides...>> final {
	using Schema = FixedArray<Sch,AddedSides...>;
};
}


template<typename Schema, uint64_t Ghost, uint64_t... Side>
struct Chunk {
	using InnerSchema = typename impl::chunk_schema_type_helper<Sch, Ghost, Side...>::Schema;
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
