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

/**
 * length 2.56m
 * height 0.64m
 * viscosity 10^-3
 * particle diameter 0.1m
 * position at (0.16m,0.32m)
 */

// Base resolution.
//
// Kept small on purpose: the SYCL JIT has to compile the whole kernel set
// before the first step runs, and on a modest machine a large domain plus a
// large particle set will exhaust memory during that compile rather than
// during the simulation itself.
constexpr uint64_t dim_y = 32ul;
constexpr uint64_t dim_x = dim_y * 4ul;

constexpr uint64_t particle_amount = 20ul;

namespace sch {

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
	Member<VelChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">,
	Member<ScalarChunk<T,Desc>, "porosity">,
	Member<VelChunk<T,Desc>, "force">
>;

template<typename T, typename Desc>
using ParticleSpheroidGroup = ParticleGroup<T,Desc::D,particle_amount,coll::Spheroid<T>>;
}

}
}
