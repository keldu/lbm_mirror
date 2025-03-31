#pragma once

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<FixedArray<T,D>, "position">,
	Member<FixedArray<T,D>, "velocity">,
	Member<FixedArray<T,D>, "acceleration">,
	Member<FixedArray<T,D>, "rotate_velocity">,
	Member<FixedArray<T,D>, "rotate_acceleration">
>;

template<typename T, uint64_t D>
using ParticleMask = Struct<
	Member<Array<T,D>, "mask">
>;
}
}
}
