#pragma once

#include "../component.hpp"
#include "common.hpp"
#include "schema.hpp"
#include "particle.hpp"
#include "integrate.hpp"

namespace kel {
namespace lbm {

namespace sch {
/**
 * One resolved contact.
 *
 * `normal` is a unit vector pointing from the partner into the body the force
 * is being computed for, so the contact force on that body is along +normal.
 * `depth` is the penetration along that normal; positive means in contact.
 */
template<typename T, uint64_t D>
using ContactManifold = Struct<
	Member<Scalar<T>,   "depth">,
	Member<Vector<T,D>, "normal">,
	Member<Vector<T,D>, "point">
>;
}

/**
 * Soft-shell contact parameters.
 *
 * k_n      normal stiffness
 * zeta_n   normal damping ratio, ~0.2..0.9; critical damping is 1
 * gamma_t  tangential viscous coefficient
 * mu       Coulomb friction cap
 */
template<typename T>
struct dem_params {
	kel::lbm::data<sch::Scalar<T>> k_n;
	kel::lbm::data<sch::Scalar<T>> zeta_n;
	kel::lbm::data<sch::Scalar<T>> gamma_t;
	kel::lbm::data<sch::Scalar<T>> mu;

	dem_params(){
		k_n.at({}).set(1.0);
		zeta_n.at({}).set(0.3);
		gamma_t.at({}).set(0.0);
		mu.at({}).set(0.0);
	}

	dem_params(
		typename kel::lbm::native_data_type<T>::type k_n__,
		typename kel::lbm::native_data_type<T>::type zeta_n__,
		typename kel::lbm::native_data_type<T>::type gamma_t__ = 0.0,
		typename kel::lbm::native_data_type<T>::type mu__ = 0.0
	){
		k_n.at({}).set(k_n__);
		zeta_n.at({}).set(zeta_n__);
		gamma_t.at({}).set(gamma_t__);
		mu.at({}).set(mu__);
	}
};

/**
 * Suggests a stiffness that keeps the worst-case overlap under delta_max for
 * an impact at v_0. Energy argument: k delta^2 / 2 = m v^2 / 2.
 */
template<typename T>
constexpr typename kel::lbm::native_data_type<T>::type dem_stiffness_for(
	typename kel::lbm::native_data_type<T>::type m_eff,
	typename kel::lbm::native_data_type<T>::type v_0,
	typename kel::lbm::native_data_type<T>::type delta_max
){
	return m_eff * v_0 * v_0 / (delta_max * delta_max);
}

namespace impl {
/**
 * math::normalize divides by sqrt(dot) with no guard, so a zero vector yields
 * NaN and poisons everything downstream. Contact routinely produces
 * near-degenerate vectors, so every normalization here goes through this.
 */
template<typename T, uint64_t D>
bool safe_normalize(
	const kel::lbm::data<sch::Vector<T,D>>& in,
	kel::lbm::data<sch::Vector<T,D>>& out
){
	auto len_2 = kel::lbm::math::dot(in,in);
	auto l2 = len_2.at({}).get();
	if(!(l2 > 0)){
		return false;
	}
	auto inv = typename kel::lbm::native_data_type<T>::type(1)
		/ std::sqrt(l2);
	for(uint64_t i{0u}; i < D; ++i){
		out.at({{i}}).set(in.at({{i}}).get() * inv);
	}
	return true;
}
}

/**
 * Narrow phase. Specialized per collision shape so the voxel version can be
 * swapped in later without touching the force law.
 */
template<typename Collision>
class narrow_phase {
	static_assert(kel::lbm::always_false<Collision>, "Not supported");
};

template<typename T>
class narrow_phase<coll::Spheroid<T>> final {
public:
	/**
	 * Sphere-sphere contact.
	 *
	 * Takes the pair in a canonical order chosen by the caller. The result is
	 * then bitwise identical for both owners of the pair, which is what makes
	 * the symmetric no-atomics formulation conserve momentum exactly: IEEE
	 * subtraction is exactly antisymmetric, but only if both sides subtract in
	 * the same order.
	 *
	 * `normal` points from `hi` toward `lo`.
	 */
	template<uint64_t D, typename PRef>
	static bool contact(
		const PRef& lo,
		const PRef& hi,
		kel::lbm::data<sch::ContactManifold<T,D>>& out
	){
		auto& pos_lo = lo.template get<"rigid_body">().template get<"position">();
		auto& pos_hi = hi.template get<"rigid_body">().template get<"position">();

		auto r_lo = lo.template get<"collision">().template get<"radius">().at({}).get();
		auto r_hi = hi.template get<"collision">().template get<"radius">().at({}).get();

		kel::lbm::data<sch::Vector<T,D>> d;
		for(uint64_t i{0u}; i < D; ++i){
			d.at({{i}}) = pos_lo.at({{i}}) - pos_hi.at({{i}});
		}

		auto len_2 = kel::lbm::math::dot(d,d).at({}).get();
		auto sum_r = r_lo + r_hi;

		if(!(len_2 < sum_r * sum_r)){
			return false;
		}

		auto len = std::sqrt(len_2);
		auto depth = sum_r - len;

		/**
		 * Cap the penetration at the smaller radius. A pair that tunnelled
		 * would otherwise produce an unbounded impulse and blow up the run.
		 */
		auto cap = (r_lo < r_hi) ? r_lo : r_hi;
		if(depth > cap){
			depth = cap;
		}

		auto& n = out.template get<"normal">();
		if(!impl::safe_normalize<T,D>(d,n)){
			/**
			 * Coincident centres: there is no geometric direction left, so
			 * pick a fixed axis. Deterministic, and it separates the pair
			 * instead of producing NaN.
			 */
			for(uint64_t i{0u}; i < D; ++i){
				n.at({{i}}).set(0);
			}
			n.at({{0u}}).set(1);
		}

		out.template get<"depth">().at({}).set(depth);

		// Contact point: on the surface of hi, along the normal.
		auto& point = out.template get<"point">();
		for(uint64_t i{0u}; i < D; ++i){
			point.at({{i}}).set(
				pos_hi.at({{i}}).get() + n.at({{i}}).get() * (r_hi - depth * 0.5)
			);
		}

		return true;
	}
};

/**
 * Force and torque produced by one contact, on one body.
 */
template<typename T, uint64_t D>
struct contact_response {
	kel::lbm::data<sch::Vector<T,D>> force;
	kel::lbm::data<typename sch::impl::rotation_type_helper<T,D>::Schema> torque;
};

namespace impl {
/**
 * The soft-shell law itself, shared by particle-particle and particle-wall.
 *
 * `m_eff` is the reduced mass of the pair; a wall is the infinite-mass limit,
 * where it collapses to the particle's own mass. `v_rel` is the relative
 * surface velocity at the contact point.
 */
template<typename T, uint64_t D>
contact_response<T,D> soft_shell_response(
	const kel::lbm::data<sch::ContactManifold<T,D>>& m,
	const kel::lbm::data<sch::Vector<T,D>>& r_self,
	const kel::lbm::data<sch::Vector<T,D>>& v_rel,
	typename kel::lbm::native_data_type<T>::type m_eff,
	const dem_params<T>& dem
){
	contact_response<T,D> res;

	auto& n = m.template get<"normal">();
	auto depth = m.template get<"depth">().at({}).get();

	auto k_n = dem.k_n.at({}).get();
	auto zeta = dem.zeta_n.at({}).get();

	// Critical damping is 2 sqrt(m k); zeta scales it.
	auto g_n = 2.0 * zeta * std::sqrt(m_eff * k_n);

	auto v_n = kel::lbm::math::dot(v_rel,n).at({}).get();

	/**
	 * Clamped at zero: a contact pushes, never pulls. Without this the
	 * dashpot would suck separating particles back together.
	 */
	auto f_n = k_n * depth - g_n * v_n;
	if(f_n < 0){
		f_n = 0;
	}

	// Tangential part of the relative velocity.
	kel::lbm::data<sch::Vector<T,D>> v_t;
	for(uint64_t i{0u}; i < D; ++i){
		v_t.at({{i}}).set(v_rel.at({{i}}).get() - v_n * n.at({{i}}).get());
	}

	kel::lbm::data<sch::Vector<T,D>> t_hat;
	auto have_t = safe_normalize<T,D>(v_t,t_hat);

	auto v_t_len = std::sqrt(kel::lbm::math::dot(v_t,v_t).at({}).get());
	auto f_t = dem.gamma_t.at({}).get() * v_t_len;
	auto f_t_cap = dem.mu.at({}).get() * f_n;
	if(f_t > f_t_cap){
		f_t = f_t_cap;
	}

	for(uint64_t i{0u}; i < D; ++i){
		auto v = f_n * n.at({{i}}).get();
		if(have_t){
			// Friction opposes sliding.
			v -= f_t * t_hat.at({{i}}).get();
		}
		res.force.at({{i}}).set(v);
	}

	// cross gives Scalar in 2D and Vector3 in 3D, which is exactly the
	// rotation type in each case, so one spelling covers both.
	res.torque = kel::lbm::math::cross<T>(r_self,res.force);

	return res;
}

/**
 * Surface velocity of a body at an offset from its centre.
 *
 * Velocity is implicit in position-Verlet, recovered from the position pair.
 */
template<typename T, uint64_t D, typename PRef>
kel::lbm::data<sch::Vector<T,D>> surface_velocity(
	const PRef& p,
	const kel::lbm::data<sch::Vector<T,D>>& r,
	typename kel::lbm::native_data_type<T>::type steps
){
	auto& rb = p.template get<"rigid_body">();
	auto& pos = rb.template get<"position">();
	auto& pos_old = rb.template get<"position_old">();

	kel::lbm::data<sch::Vector<T,D>> v;
	for(uint64_t i{0u}; i < D; ++i){
		v.at({{i}}).set((pos.at({{i}}).get() - pos_old.at({{i}}).get()) * steps);
	}

	auto& rot = rb.template get<"rotation">();
	auto& rot_old = rb.template get<"rotation_old">();

	if constexpr ( D == 2u ){
		kel::lbm::data<sch::Scalar<T>> omega;
		omega.at({}).set((rot.at({}).get() - rot_old.at({}).get()) * steps);
		auto tang = kel::lbm::math::cross<T>(omega,r);
		for(uint64_t i{0u}; i < D; ++i){
			v.at({{i}}) = v.at({{i}}) + tang.at({{i}});
		}
	}else{
		kel::lbm::data<sch::Vector<T,3u>> omega;
		for(uint64_t i{0u}; i < 3u; ++i){
			omega.at({{i}}).set((rot.at({{i}}).get() - rot_old.at({{i}}).get()) * steps);
		}
		auto tang = kel::lbm::math::cross<T>(omega,r);
		for(uint64_t i{0u}; i < D; ++i){
			v.at({{i}}) = v.at({{i}}) + tang.at({{i}});
		}
	}

	return v;
}
}

/**
 * Contact response on `self` from `other`.
 *
 * `self_is_lo` says which side of the canonical pair ordering `self` is on.
 * The manifold is always computed as (lo,hi) so both owners see bitwise
 * identical geometry; the hi owner simply flips the normal.
 */
template<typename T, uint64_t D, typename PRef>
contact_response<T,D> particle_contact_response(
	const kel::lbm::data<sch::ContactManifold<T,D>>& m_in,
	const PRef& self,
	const PRef& other,
	bool self_is_lo,
	const dem_params<T>& dem,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto steps = static_cast<typename kel::lbm::native_data_type<T>::type>(
		particle_sub_steps(sub_steps));

	kel::lbm::data<sch::ContactManifold<T,D>> m = m_in;
	if(!self_is_lo){
		auto& n = m.template get<"normal">();
		for(uint64_t i{0u}; i < D; ++i){
			n.at({{i}}).set(-n.at({{i}}).get());
		}
	}

	auto& point = m.template get<"point">();
	auto& pos_s = self.template get<"rigid_body">().template get<"position">();
	auto& pos_o = other.template get<"rigid_body">().template get<"position">();

	kel::lbm::data<sch::Vector<T,D>> r_s, r_o;
	for(uint64_t i{0u}; i < D; ++i){
		r_s.at({{i}}).set(point.at({{i}}).get() - pos_s.at({{i}}).get());
		r_o.at({{i}}).set(point.at({{i}}).get() - pos_o.at({{i}}).get());
	}

	auto v_s = impl::surface_velocity<T,D>(self,r_s,steps);
	auto v_o = impl::surface_velocity<T,D>(other,r_o,steps);

	kel::lbm::data<sch::Vector<T,D>> v_rel;
	for(uint64_t i{0u}; i < D; ++i){
		v_rel.at({{i}}).set(v_s.at({{i}}).get() - v_o.at({{i}}).get());
	}

	auto m_s = self.template get<"rigid_body">().template get<"mass">().at({}).get();
	auto m_o = other.template get<"rigid_body">().template get<"mass">().at({}).get();
	auto m_eff = (m_s * m_o) / (m_s + m_o);

	return impl::soft_shell_response<T,D>(m,r_s,v_rel,m_eff,dem);
}

/**
 * Contact response against a static wall.
 *
 * A wall is a partner of infinite mass and zero velocity, so the reduced mass
 * collapses to the particle's own and the relative velocity is just the
 * particle's surface velocity. The force law is otherwise identical -- the
 * same soft shell.
 */
template<typename T, uint64_t D, typename PRef>
contact_response<T,D> wall_contact_response(
	const kel::lbm::data<sch::ContactManifold<T,D>>& m,
	const PRef& self,
	const dem_params<T>& dem,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto steps = static_cast<typename kel::lbm::native_data_type<T>::type>(
		particle_sub_steps(sub_steps));

	auto& point = m.template get<"point">();
	auto& pos_s = self.template get<"rigid_body">().template get<"position">();

	kel::lbm::data<sch::Vector<T,D>> r_s;
	for(uint64_t i{0u}; i < D; ++i){
		r_s.at({{i}}).set(point.at({{i}}).get() - pos_s.at({{i}}).get());
	}

	auto v_rel = impl::surface_velocity<T,D>(self,r_s,steps);

	auto m_s = self.template get<"rigid_body">().template get<"mass">().at({}).get();

	return impl::soft_shell_response<T,D>(m,r_s,v_rel,m_s,dem);
}

/**
 * Accumulates wall contacts for the axis-aligned domain box.
 *
 * One manifold per face the particle has entered; a particle in a corner gets
 * a contribution from each axis, which is what makes it settle into the corner
 * rather than squeeze through it.
 */
template<typename T, uint64_t D, typename PRef>
void accumulate_box_wall_contacts(
	PRef&& self,
	const kel::lbm::data<sch::FixedArray<sch::UInt64,D>>& extent,
	const dem_params<T>& dem,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto& rb = self.template get<"rigid_body">();
	auto& pos = rb.template get<"position">();
	auto r = self.template get<"collision">().template get<"radius">().at({}).get();

	for(uint64_t ax{0u}; ax < D; ++ax){
		auto x = pos.at({{ax}}).get();
		auto hi_bound = static_cast<typename kel::lbm::native_data_type<T>::type>(
			extent.at({ax}).get());

		// Low face, inward normal +e_ax; high face, inward normal -e_ax.
		for(uint64_t side{0u}; side < 2u; ++side){
			auto depth = (side == 0u) ? (r - x) : (x - (hi_bound - r));
			if(!(depth > 0)){
				continue;
			}
			if(depth > r){
				depth = r;
			}

			kel::lbm::data<sch::ContactManifold<T,D>> m;
			auto& n = m.template get<"normal">();
			auto& point = m.template get<"point">();
			for(uint64_t i{0u}; i < D; ++i){
				n.at({{i}}).set(0);
				point.at({{i}}).set(pos.at({{i}}).get());
			}
			n.at({{ax}}).set((side == 0u) ? 1.0 : -1.0);
			point.at({{ax}}).set((side == 0u) ? 0.0 : hi_bound);
			m.template get<"depth">().at({}).set(depth);

			auto res = wall_contact_response<T,D>(m,self,dem,sub_steps);

			auto& force = rb.template get<"force">();
			for(uint64_t i{0u}; i < D; ++i){
				force.at({{i}}) = force.at({{i}}) + res.force.at({{i}});
			}
			if constexpr ( particle_angular_supported<D> ){
				auto& torque = rb.template get<"torque">();
				torque = torque + res.torque;
			}
		}
	}
}

/**
 * Brute-force pair loop: accumulates every contact acting on one particle.
 *
 * Only `self` is written. The partner's half is computed by the partner's own
 * work-item, which is what removes the need for atomics. The pair geometry is
 * evaluated in canonical (lo,hi) order on both sides so the two halves are
 * exactly equal and opposite.
 */
template<typename T, uint64_t D, typename PartsRef>
void accumulate_particle_contacts(
	PartsRef&& parts,
	uint64_t self_index,
	const dem_params<T>& dem,
	kel::lbm::data<sch::UInt64> sub_steps
){
	auto& self = parts.at({self_index});

	// get_dims() is the one spelling both the host array and the device Ptr view share.
	const uint64_t count = parts.get_dims().at({0u}).get();

	for(uint64_t j{0u}; j < count; ++j){
		if(j == self_index){
			continue;
		}

		auto& other = parts.at({j});

		if(!(broadphase_collision_check<T,D,coll::Spheroid<T>>)(self,other)){
			continue;
		}

		const bool self_is_lo = (self_index < j);
		auto& lo = self_is_lo ? self : other;
		auto& hi = self_is_lo ? other : self;

		kel::lbm::data<sch::ContactManifold<T,D>> m;
		if(!narrow_phase<coll::Spheroid<T>>::template contact<D>(lo,hi,m)){
			continue;
		}

		auto res = particle_contact_response<T,D>(m,self,other,self_is_lo,dem,sub_steps);

		auto& rb = self.template get<"rigid_body">();
		auto& force = rb.template get<"force">();
		for(uint64_t i{0u}; i < D; ++i){
			force.at({{i}}) = force.at({{i}}) + res.force.at({{i}});
		}
		// Tied to the integrator: accumulating torque the integrator will not
		// consume would be a silent no-op.
		if constexpr ( particle_angular_supported<D> ){
			auto& torque = rb.template get<"torque">();
			torque = torque + res.torque;
		}
	}
}

namespace cmpt {
/**
 * Accumulates contact forces onto one particle. Runs between the fluid gather
 * and the integrate.
 */
struct ParticleContact {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::ParticleContact, Encode> final {
public:
	using Component = cmpt::ParticleContact;

private:
	dem_params<T> dem_;
	bool with_walls_;
	kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> extent_;

public:
	component():
		dem_{}, with_walls_{false}, extent_{}
	{}

	component(const dem_params<T>& dem__):
		dem_{dem__}, with_walls_{false}, extent_{}
	{}

	component(
		const dem_params<T>& dem__,
		const kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& extent__
	):
		dem_{dem__}, with_walls_{true}, extent_{extent__}
	{}

	template<typename ParticleSchema>
	void apply(
		const kel::lbm::data<ParticleSchema,Encode>& pg,
		kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index,
		kel::lbm::data<sch::UInt64> time_step,
		kel::lbm::data<sch::UInt64> sub_steps
	) const {
		(void) time_step;

		auto& parts = pg.template get<"particles">();

		accumulate_particle_contacts<T,Descriptor::D>(
			parts, index.at({{0u}}).get(), dem_, sub_steps);

		if(with_walls_){
			accumulate_box_wall_contacts<T,Descriptor::D>(
				parts.at(index), extent_, dem_, sub_steps);
		}
	}
};

}
}
