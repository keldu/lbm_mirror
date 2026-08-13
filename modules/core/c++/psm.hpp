#pragma once

#include "macroscopic.hpp"
#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct PsmReset {};
struct PsmOneParticle {};
struct Psm {};
}

template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PsmReset, Encode> final {
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
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
	saw::data<T> relaxation_;
	saw::data<T> frequency_;
public:
	component(
		typename saw::native_data_type<T>::type relaxation__
	):
		relaxation_{relaxation__}
	{
		saw::data<T> one;
		one = 1.0;
		frequency_ = one / relaxation_;
	}

	template<typename CellFieldSchema, typename MacroFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<MacroFieldSchema,Encode>& macros, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
	
		using dfi = df_info<T,Descriptor>;
		bool is_even = ((time_step.get() % 2) == 0);

		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		auto& porous_f = macros.template get<"porosity">();

		auto& rho_f = macros.template get<"density">();
		auto& vel_f = macros.template get<"momentum">();
		
		saw::data<sch::Scalar<T>>& rho = rho_f.at(index);
		saw::data<sch::Vector<T,Descriptor::D>>& vel = vel_f.at(index);

		compute_rho_u<T,Descriptor>(dfs_old_f.at(index),rho,vel);
		
		auto eq = equilibrium<T,Descriptor>(rho,vel);

		saw::data<T> one{1.0};
		auto& porous = porous_f.at(index);
		auto flip_porous = one - porous.at({});

		auto& dfs = dfs_old_f.at(index);

		auto dfs_cpy = dfs;

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			uint64_t i_opp = dfi::opposite_index[i];
			dfs.at({i}) = dfs_cpy.at({i}) + frequency_ * (eq.at(i) - dfs_cpy.at({i})) * porous.at({}) + (dfs_cpy.at({i_opp}) - dfs_cpy.at({i}) ) * flip_porous;
		}

		auto& force_f = macros.template get<"force">();
		auto& force = force_f.at(index);
		for(uint64_t k{0u}; k < Descriptor::D; ++k){
			force.at({{k}}).set(0);
		}

		for(uint64_t i{0u}; i < Descriptor::Q; ++i){
			uint64_t i_opp = dfi::opposite_index[i];
			auto dfs_diff = dfs.at({i}) - dfs.at({i_opp});
			for(uint64_t k{0u}; k < Descriptor::D; ++k){
				force.at({{k}}) = force.at({{k}}) + dfs_diff * dfi::directions[i][k];
			}
		}

		force = force * porous;
	}
};

/**
 * PSM collision operator for LBM
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::PsmOneParticle, Encode> final {
private:
public:
	component() = default;

	template<typename CellFieldSchema, typename MacroFieldSchema, typename ParticleSchema>
void apply(
    const saw::data<CellFieldSchema, Encode>& field,
    const saw::data<MacroFieldSchema, Encode>& macros,
    const saw::data<ParticleSchema, Encode>& pg,
    saw::data<sch::FixedArray<sch::UInt64,1u>> index,
    saw::data<sch::UInt64> time_step,
    saw::data<sch::UInt64> sub_steps
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

    saw::data<sch::Scalar<T>> one;
    one.at({}) = 1.0;

		saw::data<sch::Scalar<T>> half;
		half.at({}) = 0.5;

    /*
     * ------------------------------------------------------------
     * Particle
     * ------------------------------------------------------------
     */

    {
        auto parts = pg.template get<"particles">();

        auto& p_coll =
            pg.template get<"collision">().at({});

        auto& p_rad =
            p_coll.template get<"radius">();

        auto& pi = parts.at(index);

        auto& pirb =
            pi.template get<"rigid_body">();

        saw::data<sch::Vector<T,Descriptor::D>>& pirb_pos =
            pirb.template get<"position">();

        auto& pirb_pos_old =
            pirb.template get<"position_old">();


        /*
         * --------------------------------------------------------
         * Particle timestep
         * --------------------------------------------------------
         */

        saw::data<sch::Scalar<T>> ts;

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

        auto eo_aabb =
            particle_aabb<
                typename ParticleSchema::ValueType
            >::calculate(
                pg,
                {{0u}},
                vel_f.meta()
            );

        if(eo_aabb.is_error()){
            return;
        }

        auto& aabb = eo_aabb.get_value();

        saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> start;
        saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> stop;

        start = aabb.template get<"a">();
        stop  = aabb.template get<"b">();


        /*
         * --------------------------------------------------------
         * Total force acting on particle
         * --------------------------------------------------------
         */

        saw::data<sch::Vector<T,Descriptor::D>> force_p;

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

                saw::data<sch::Vector<T,Descriptor::D>> rel_dist =
                    saw::math::vectorize_data(index_f)
                    .template cast_to<T>()
                    - pirb_pos;


                /*
                 * ------------------------------------------------
                 * PSM particle porosity
                 * ------------------------------------------------
                 */

                saw::data<sch::Scalar<T>> eps;
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

                saw::data<sch::Vector<T,Descriptor::D>> momentum;

                for(uint64_t k = 0u; k < Descriptor::D; ++k){
                    momentum.at({{k}}) = 0.0;
                }


                /*
                 * ------------------------------------------------
                 * D3Q27 / general Q momentum exchange
                 *
                 * We exclude i=0 because the rest population has
                 * no momentum.
                 *
                 * The original code summed both directions of
                 * every opposite pair. Therefore we apply 1/2
                 * after the sum.
                 * ------------------------------------------------
                 */

                for(uint64_t i = 1u; i < Descriptor::Q; ++i){

                    uint64_t i_opp =
                        dfi::opposite_index[i];


                    /*
                     * Direction vector e_i.
                     */

                    saw::data<sch::Vector<T,Descriptor::D>> e_i;

                    saw::data<
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
                        saw::math::dot(
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

                    saw::data<T> dfs_added =
                          dfs.at({i})
                            * (
                                saw::data<T>{1.0}
                                - u_p_e.at({})
                            )

                        + dfs_old_f
                            .at(n_ind_i)
                            .at({i_opp})
                            * (
                                saw::data<T>{1.0}
                                + u_p_e.at({})
                            );


                    /*
                     * Convert scalar into vector multiplication.
                     */

                    saw::data<sch::Scalar<T>> dfs_added_v;

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


                /*
                 * ------------------------------------------------
                 * Since both i and i_opp were included above,
                 * compensate for double counting.
                 *
                 * This is the important change from your original
                 * implementation.
                 * ------------------------------------------------
                 */



                momentum =
                    momentum * half;


                /*
                 * ------------------------------------------------
                 * PSM solid fraction
                 *
                 * por = 1   -> fluid
                 * por = 0   -> particle
                 *
                 * Therefore:
                 *
                 * flip_por = 1 - por
                 * ------------------------------------------------
                 */

                auto flip_por =
                    one - por;


                /*
                 * ------------------------------------------------
                 * Force transferred TO FLUID
                 * ------------------------------------------------
                 */

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

        auto& pirb_acc =
            pirb.template get<"acceleration">();

        pirb_acc =
            force_p
            / pg.template get<"total_mass">().at({});


        /*
         * ------------------------------------------------------------
         * Integrate particle
         * ------------------------------------------------------------
         */

        for(saw::data<sch::UInt64> i{0u};
            i < sub_steps;
            ++i){

            verlet_step_lambda<T,Descriptor::D>(
                pi,
                ts
            );
        }
    }
}
};

}
}
