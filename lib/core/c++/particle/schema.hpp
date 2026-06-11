#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

namespace coll {
template<typename T>
struct Spheroid {
	using ValueSchema = T;
	using Schema = sch::Struct<
		sch::Member<sch::Scalar<ValueSchema>,"radius">
	>;
};
}

namespace sch {
using namespace saw::schema;

namespace impl {
template<typename T,uint64_t D>
struct rotation_type_helper;

template<typename T>
struct rotation_type_helper<T,2u> {
	using Schema = Scalar<T>;
};

template<typename T>
struct rotation_type_helper<T,3u> {
	using Schema = Vector<T,3u>;
};
}

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	Member<Vector<T,D>, "position">,
	Member<Vector<T,D>, "position_old">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation_old">,

	Member<Vector<T,D>, "acceleration">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "angular_acceleration">
>;


template<typename T, uint64_t D>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">
	// Problem is that dynamic data would two layered
	// Member<Array<Float64,D>, "mask">,
>;

template<typename T, uint64_t D, typename CollisionType = coll::Spheroid<T>>
using ParticleGroup = Struct<
	Member<Array<T,D>, "mask">,
	Member<FixedArray<typename CollisionType::Schema,1u>, "collision">,
	Member<FixedArray<Scalar<T>,1u>, "mask_step">,
	Member<FixedArray<Scalar<T>,1u>, "density">,
	Member<FixedArray<Vector<T,D>,1u>, "center_of_mass">,
	Member<FixedArray<Scalar<T>,1u>, "total_mass">,
	Member<Array<Particle<T,D>,1u>, "particles">
>;
}
}
}
