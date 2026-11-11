#pragma once

#include "common.hpp"

#include "../lbm_unit.hpp"

namespace kel {
namespace lbm {

namespace coll {
template<typename T>
struct Spheroid {
	using ValueSchema = T;
	using Schema = sch::Struct<
		// A radius is a lattice length, like the position it is compared against.
		sch::Member<sch::LbmMeter<sch::Scalar<ValueSchema>>,"radius">
	>;
};
}

namespace sch {

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

/**
 * Moment of inertia.
 *
 * A scalar is exact in 2D, where rotation is about the single out-of-plane
 * axis. In 3D it is only correct for isotropic bodies -- anything else needs
 * a body-frame tensor plus the gyroscopic term. The angular DEM path
 * static_asserts on D == 2u so 3D fails loudly rather than silently wrong.
 */
template<typename T,uint64_t D>
struct inertia_type_helper {
	using Schema = Scalar<T>;
};
}

template<typename T, uint64_t D>
using ParticleRigidBody = Struct<
	/**
	 * Lattice length. The unit sits on the vector rather than on its
	 * components: a position is metres in every direction, and the component
	 * type is what the kernels index.
	 */
	Member<LbmMeter<Vector<T,D>>, "position">,
	Member<LbmMeter<Vector<T,D>>, "position_old">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "rotation_old">,

	/**
	 * Lattice acceleration. Typed because the Verlet update adds
	 * acceleration * dt^2 to a position and so must come out in metres.
	 * It is also where the typed and untyped halves meet: force and mass have
	 * no units yet, so integrate_particle attaches this one explicitly.
	 */
	Member<LbmAcceleration<Vector<T,D>>, "acceleration">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "angular_acceleration">,

	/**
	 * Accumulators, zeroed at the start of every step.
	 *
	 * Every contributor -- fluid coupling, particle contact, body forces --
	 * adds into these; a single integrate pass then turns them into
	 * acceleration. Writing acceleration directly, as the fluid components
	 * used to, means the last writer silently wins.
	 */
	Member<Vector<T,D>, "force">,
	Member<typename impl::rotation_type_helper<T,D>::Schema, "torque">,

	/**
	 * Inertial properties, per particle rather than per group, so a group can
	 * hold particles of differing size and density.
	 */
	Member<Scalar<T>, "mass">,
	Member<typename impl::inertia_type_helper<T,D>::Schema, "inertia">,
	Member<Scalar<T>, "density">
>;

/**
 * CollisionType is defaulted so the plain `Particle<T,D>` spelling used by the
 * integrator and the tests keeps working.
 */
template<typename T, uint64_t D, typename CollisionType = coll::Spheroid<T>>
using Particle = Struct<
	Member<ParticleRigidBody<T,D>, "rigid_body">,
	Member<typename CollisionType::Schema, "collision">
>;

/**
 * A group is one shape shared by many particles: the voxel mask and its
 * sampling parameters describe the shape, the particles carry the transform
 * and the dynamics. Multiple groups give multiple shapes.
 */
template<typename T, uint64_t D, uint64_t PartAmount, typename CollisionType = coll::Spheroid<T>>
using ParticleGroup = Struct<
	Member<Array<T,D>, "mask">,
	Member<FixedArray<typename CollisionType::Schema,1u>, "collision">,
	Member<FixedArray<Scalar<T>,1u>, "epsilon">,
	Member<FixedArray<Scalar<T>,1u>, "mask_step">,
	Member<FixedArray<Vector<T,D>,1u>, "center_of_mass">,
	Member<FixedArray<Particle<T,D,CollisionType>,PartAmount>, "particles">
>;
}
}
}
