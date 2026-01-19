#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {
namespace sch {
namespace impl {
template<typename Sch, uint64_t Dim, uint64_t Side, typename... AddedSides>
struct chunk_schema_type_helper {
	using Schema = typename chunk_schema_type_helper<Sch,Dim-1u,Side,AddedSides...,Side>::Schema;
};

template<typename Sch, uint64_t Side, typename... AddedSides>
struct chunk_schema_type_helper<Sch, 0u, Side, AddedSides...> {
	using Schema = FixedArray<Sch,AddedSides...>;
};
}


template<typename Schema, uint64_t Dim, uint64_t Side, uint64_t Ghost = 0u>
struct Chunk {
	using InnerSchema = typename impl::chunk_schema_type_helper<Sch, Dim, Side + (2u*Ghost)>::Schema;
};

template<typename ChunkSchema, uint64_t Dim>
using SuperChunk = Array<ChunkSchema,Dim>;
}
}
}

namespace saw {
template<typename Sch, uint64_t Dim, uint64_t Side, uint64_t Ghost, typename Encode>
class data<kel::lbm::sch::Chunk<Sch,Dim,Side,Ghost>,Encode> final {
private:
public:
};
}
