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
 * length 2.2m
 * height 0.41m
 * viscosity 10^-3
 * particle diameter 0.1m
 * position at (0.16m,0.2m)
 * velocity of 0.3 m/s from 1.2/0.41 * x - 1.2/0.41² * x² at x=0.41/2
 */

constexpr uint64_t dim_y = 41u;
constexpr uint64_t dim_x = 220u;

template<uint64_t N>
struct dimensions {
	static constexpr std::array<uint64_t,2u> calculate(){
		return {dim_x * N + 1u, dim_y * N + 1u};
	}
};

template<uint64_t N>
constexpr std::array<uint64_t,2u> dims{
	dim_x * N + 1u,
	dim_y * N + 1u
};

constexpr uint64_t particle_amount = 1ul;

namespace sch {
using namespace saw::schema;

template<uint64_t N>
using InfoChunk = Chunk<UInt8, 0u, dim_x*N, dim_y*N>;

template<typename T, typename Desc, uint64_t N>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x*N, dim_y*N>;

template<typename T, typename Desc, uint64_t N>
using ScalarChunk = Chunk<Scalar<T>, 0u, dim_x*N, dim_y*N>;

template<typename T, typename Desc, uint64_t N>
using VectorChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x*N, dim_y*N>;

template<typename T, typename Desc, uint64_t N = 1u>
using ChunkStruct = Struct<
	Member<InfoChunk<N>, "info">,
	Member<DfChunk<T,Desc,N>, "dfs">,
	Member<DfChunk<T,Desc,N>, "dfs_old">,
	Member<VectorChunk<T,Desc,N>, "particle_N">,
	Member<ScalarChunk<T,Desc,N>, "particle_D">
>;

template<typename T, typename Desc, uint64_t N>
using VelChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x*N, dim_y*N>;

template<typename T, uint64_t N>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x*N, dim_y*N>;

template<typename T, typename Desc, uint64_t N = 1u>
using MacroStruct = Struct<
	Member<VelChunk<T,Desc,N>, "momentum">,
	Member<RhoChunk<T,N>, "density">,
	Member<ScalarChunk<T,Desc,N>, "porosity">,
	Member<VelChunk<T,Desc,N>, "force">
>;

template<typename T, typename Desc>
using ParticleSpheroidGroup = ParticleGroup<T,Desc::D,1u,coll::Spheroid<T>>;
}

}
}
