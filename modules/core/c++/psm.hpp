#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"
#include "particle/integrate.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct PsmReset {};
struct PsmOneParticle {};
struct PsmOneParticleGather {};
struct Psm {};
}
namespace method {
struct Psm {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PsmReset, Encode> final {
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, kel::lbm::data<sch::UInt64> time_step) const {
		auto& porosity_f = macros.template get<"porosity">();

		auto& por = porosity_f.at(index);
		por.at({}) = 1.0;
	}
};

/**
 * PSM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::Psm, Encode> {
private:
	kel::lbm::data<T> relaxation_;
	kel::lbm::data<T> frequency_;
public:
	component(
		typename kel::lbm::native_data_type<T>::type relaxation__
	):
		relaxation_{relaxation__}
	{
		kel::lbm::data<T> one;
		one = 1.0;
		frequency_ = one / relaxation_;
	}

	component(const kel::lbm::data<sch::Pure<T>>& relaxation__):
		relaxation_{relaxation__.handle()},
		frequency_{kel::lbm::data<T>{typename kel::lbm::native_data_type<T>::type(1)} / relaxation_}
	{}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const kel::lbm::data<CellFieldSchema, Encode>& field, const kel::lbm::data<MacroFieldSchema,Encode>& macros, kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, kel::lbm::data<sch::UInt64> time_step) const {
	
		using dfi = df_info<T,Descriptor>;
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& porous_f = macros.template get<"porosity">();

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"momentum">();
		
		kel::lbm::data<sch::Scalar<T>>& rho = rho_f.at(index);
		kel::lbm::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		kel::lbm::data<T> one{1.0};
		auto& porous = porous_f.at(index);
		auto flip_porous = one - porous.at({});

		auto& dfs = dfs_old_f.at(index);

		auto dfs_cpy = dfs;

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			uint64_t i_opp = dfi::opposite_index[i];
			dfs.at({i}) = dfs_cpy.at({i}) + frequency_ * (eq.at(i) - dfs_cpy.at({i})) * porous.at({}) + (dfs_cpy.at({i_opp}) - dfs_cpy.at({i}) ) * flip_porous;
		}
	}
};

/**
 * PSM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PsmOneParticleGather, Encode> final {
private:
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
void apply(
    const kel::lbm::data<CellFieldSchema, Encode>& field,
    const kel::lbm::data<MacroFieldSchema, Encode>& macros,
    const kel::lbm::data<ParticleSchema, Encode>& pg,
    kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index,
    kel::lbm::data<sch::UInt64> time_step,
    kel::lbm::data<sch::UInt64> sub_steps
) const {

    using dfi = df_info<T,Descriptor>;

    bool is_even = ((time_step.get() % 2u) == 0u);

    /*
     * ------------------------------------------------------------
     * Fields
     * ------------------------------------------------------------
     */

    auto& dfs_old_f =
        (is_even)
            ? field.template get<"dfs_old">()
            : field.template get<"dfs">();

    auto& por_f   = macros.template get<"porosity">();
    auto& rho_f   = macros.template get<"density">();
    auto& vel_f   = macros.template get<"momentum">();
    auto& force_f = macros.template get<"force">();

    kel::lbm::data<sch::Scalar<T>> one;
    one.at({}) = 1.0;

		kel::lbm::data<sch::Scalar<T>> half;
		half.at({}) = 0.5;

    /*
     * ------------------------------------------------------------
     * Particle
     * ------------------------------------------------------------
     */

    {
        auto parts = pg.template get<"particles">();
        auto& pi = parts.at(index);

        auto& p_rad =
            pi.template get<"collision">().template get<"radius">();

        auto& pirb =
            pi.template get<"rigid_body">();

        kel::lbm::data<sch::Vector<T,Descriptor::D>>& pirb_pos =
            pirb.template get<"position">();

        auto& pirb_pos_old =
            pirb.template get<"position_old">();


        /*
         * --------------------------------------------------------
         * Particle timestep
         * --------------------------------------------------------
         */

        kel::lbm::data<sch::Scalar<T>> ts;

        ts.at({}) =
            one.at({})
            / sub_steps.template cast_to<T>();


        /*
         * --------------------------------------------------------
         * Particle momentum
         * --------------------------------------------------------
         */

        auto vel_p_old =
            (pirb_pos - pirb_pos_old) / ts;


        /*
         * --------------------------------------------------------
         * Find particle AABB
         * --------------------------------------------------------
         */

        kel::lbm::data<typename particle_aabb<typename ParticleSchema::ValueType>::AABB> aabb;
        if(not particle_aabb<typename ParticleSchema::ValueType>::try_calculate(pg,index,vel_f.meta(),aabb)){
            return;
        }

        kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> start;
        kel::lbm::data<sch::FixedArray<sch::UInt64,Descriptor::D>> stop;

        start = aabb.template get<"a">();
        stop  = aabb.template get<"b">();


        /*
         * --------------------------------------------------------
         * Total force acting on particle
         * --------------------------------------------------------
         */

        kel::lbm::data<sch::Vector<T,Descriptor::D>> force_p;

        for(uint64_t k = 0u; k < Descriptor::D; ++k){
            force_p.at({{k}}) = 0.0;
        }


        /*
         * --------------------------------------------------------
         * Iterate over particle AABB
         * --------------------------------------------------------
         */

        iterator<Descriptor::D>::apply(
            [&](const auto& index_f) -> void {

                /*
                 * ------------------------------------------------
                 * Distribution function at current cell
                 * ------------------------------------------------
                 */

                auto& dfs =
                    dfs_old_f.at(index_f);


                /*
                 * ------------------------------------------------
                 * Particle relative position
                 * ------------------------------------------------
                 */

                kel::lbm::data<sch::Vector<T,Descriptor::D>> rel_dist =
                    kel::lbm::math::vectorize_data(index_f)
                    .template cast_to<T>()
                    - pirb_pos;


                /*
                 * ------------------------------------------------
                 * PSM particle porosity
                 * ------------------------------------------------
                 */

                kel::lbm::data<sch::Scalar<T>> eps;
                eps.at({}) = 1.5;

                auto& por =
                    por_f.at(index_f);

                por =
                    particle_porosity<
                        T,
                        Descriptor::D,
                        1u,
                        por::ParticleSpheroid<T>
                    >::calculate(
                        rel_dist,
                        p_rad,
                        eps
                    );


                /*
                 * Outside particle.
                 */

                if(por.at({}).get() >= 1.0){
                    return;
                }


                /*
                 * ------------------------------------------------
                 * Momentum exchange for this cell
                 * ------------------------------------------------
                 */

                kel::lbm::data<sch::Vector<T,Descriptor::D>> momentum;

                for(uint64_t k = 0u; k < Descriptor::D; ++k){
                    momentum.at({{k}}) = 0.0;
                }


                /*
                 * ------------------------------------------------
                 * We exclude i=0 because the rest population has
                 * no momentum.
                 *
                 * ------------------------------------------------
                 */

                for(uint64_t i = 1u; i < Descriptor::Q; ++i){

                    uint64_t i_opp =
                        dfi::opposite_index[i];


                    /*
                     * Direction vector e_i.
                     */

                    kel::lbm::data<sch::Vector<T,Descriptor::D>> e_i;

                    kel::lbm::data<
                        sch::FixedArray<
                            sch::UInt64,
                            Descriptor::D
                        >
                    > n_ind_i;


                    for(uint64_t k = 0u;
                        k < Descriptor::D;
                        ++k){

                        e_i.at({{k}}) =
                            dfi::directions[i][k];

                        /*
                         * Neighbor in direction e_i.
                         */

                        n_ind_i.at({k}) =
                            index_f.at({k})
                            + dfi::directions[i][k];
                    }


                    /*
                     * ------------------------------------------------
                     * Particle momentum projected onto e_i
                     * ------------------------------------------------
                     */

                    auto u_p_e =
                        kel::lbm::math::dot(
                            e_i,
                            vel_p_old
                        );


                    /*
                     * ------------------------------------------------
                     * Momentum-exchange population
                     *
                     * f_i(x)
                     *
                     * + f_-i(x + e_i)
                     *
                     * with moving-wall correction.
                     * ------------------------------------------------
                     */

                    kel::lbm::data<T> dfs_added =
                          dfs.at({i})
                            * (
                                kel::lbm::data<T>{1.0}
                                - u_p_e.at({})
                            )

                        + dfs_old_f
                            .at(n_ind_i)
                            .at({i_opp})
                            * (
                                kel::lbm::data<T>{1.0}
                                + u_p_e.at({})
                            );


                    /*
                     * Convert scalar into vector multiplication.
                     */

                    kel::lbm::data<sch::Scalar<T>> dfs_added_v;

                    dfs_added_v.at({}) =
                        dfs_added;


                    /*
                     * ------------------------------------------------
                     * Delta p_i
                     *
                     * ------------------------------------------------
                     */

                    momentum =
                        momentum
                        + e_i * dfs_added_v;
                }

                auto flip_por =
                    one - por;


                auto& force =
                    force_f.at(index_f);

                force =
                    momentum * flip_por;


                /*
                 * ------------------------------------------------
                 * Force transferred TO PARTICLE
                 *
                 * Newton's third law.
                 * ------------------------------------------------
                 */

                force_p =
                    force_p - force;

            },
            start,
            stop
        );


        /*
         * ------------------------------------------------------------
         * Particle acceleration
         * ------------------------------------------------------------
         */

        /**
         * Accumulate rather than assign, so a contact pass can add to this
         * before cmpt::ParticleIntegrate turns the total into motion.
         */
        auto& pirb_force = pirb.template get<"force">();
        pirb_force = pirb_force + force_p;
    }
}
};

/**
 * The original fused one-particle step, kept so existing call sites keep
 * working.
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PsmOneParticle, Encode> final {
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
	void apply(
		const kel::lbm::data<CellFieldSchema, Encode>& field,
		const kel::lbm::data<MacroFieldSchema, Encode>& macros,
		const kel::lbm::data<ParticleSchema, Encode>& pg,
		kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index,
		kel::lbm::data<sch::UInt64> time_step,
		kel::lbm::data<sch::UInt64> sub_steps
	) const {
		component<T,Descriptor,cmpt::ParticleForceReset,Encode> reset;
		component<T,Descriptor,cmpt::PsmOneParticleGather,Encode> gather;
		component<T,Descriptor,cmpt::ParticleIntegrate,Encode> integrate;

		reset.apply(pg,index,time_step);
		gather.apply(field,macros,pg,index,time_step,sub_steps);
		integrate.apply(pg,index,time_step,sub_steps);
	}
};

}
}
