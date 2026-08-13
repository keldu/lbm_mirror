#pragma once

#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>
#include <kel/lbm/particle.hpp>
#include <kel/lbm/particle/particle.hpp>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_y = 256ul;
constexpr uint64_t dim_x = dim_y * 20ul;

constexpr uint64_t particle_amount = 1ul;

namespace sch {
using namespace saw::schema;

using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y>;

template<typename T, typename Desc>
using ScalarChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using VectorChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using ChunkStruct = Struct<
	Member<InfoChunk, "info">,
	Member<DfChunk<T,Desc>, "dfs">,
	Member<DfChunk<T,Desc>, "dfs_old">,
	Member<VectorChunk<T,Desc>, "particle_N">,
	Member<ScalarChunk<T,Desc>, "particle_D">
>;

template<typename T, typename Desc>
using VelChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<VelChunk<T,Desc>, "momentum">,
	Member<RhoChunk<T>, "density">,
	Member<ScalarChunk<T,Desc>, "porosity">
>;

template<typename T, typename Desc>
using ParticleSpheroidGroup = ParticleGroup<T,Desc::D,1u,coll::Spheroid<T>>;
}

}
}
