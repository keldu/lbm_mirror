#pragma once

#include "component.hpp"
#include "equilibrium.hpp"

namespace kel {
namespace lbm {
namespace cmpt {
struct BounceBack {};

template<uint64_t i>
struct AntiBounceBack {};

template<bool East>
struct ZouHePressureX{};

template<bool East>
using ZouHeHorizontal = ZouHePressureX<East>;

struct Equilibrium {};

template<bool North>
struct ZouHePressureY{};

template<bool East>
struct ZouHeVelocityX {};
}

/**
 * Full-Way BounceBack.
 */
template<typename T, typename Descriptor, typename Encode>
class component<T, Descriptor, cmpt::BounceBack, Encode> {
private:
public:
	component() = default;

	using Component = cmpt::BounceBack;

	/**
	 * Thoughts regarding automating order setup
	 */
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "full_way_bounce_back";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step)const{
		bool is_even = ((time_step.get() % 2u) == 0u);

		// This is a ref

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		using dfi = df_info<T,Descriptor>;

		// Technically use .copy()
		auto& dfs_old = dfs_old_f.at(index);
		auto df_cpy = dfs_old;

		for(uint64_t i = 0u; i < Descriptor::Q; ++i){
			dfs_old.at({i}) = df_cpy.at({dfi::opposite_index.at(i)});
		}
	}
};

/**
 * Anti Full-Way BounceBack.
 */
template<typename T, typename Descriptor, uint64_t D, typename Encode>
class component<T, Descriptor, cmpt::AntiBounceBack<D>, Encode> {
private:
public:
	component() = default;

	using Component = cmpt::AntiBounceBack<D>;

	/**
	 * Thoughts regarding automating order setup
	 */
	using access = cmpt::access_tuple<
		cmpt::access<"dfs", 1, true, true, true>
	>;

	/**
	 * Thoughts regarding automating order setup
	 */
	static constexpr saw::string_literal name = "full_way_anti_bounce_back";
	static constexpr saw::string_literal after = "";
	static constexpr saw::string_literal before = "streaming";

	/**
	 * Raw setup
	 */
	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step)const{
		bool is_even = ((time_step.get() % 2u) == 0u);

		// This is a ref

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		using dfi = df_info<T,Descriptor>;

		// Technically use .copy()
		auto& dfs_old = dfs_old_f.at(index);
		auto df_cpy = dfs_old;
		
		static_assert(Descriptor::D == 2u and Descriptor::Q == 9u, "Some parts are hard coded sadly");

		dfs_old.at({0u}) = df_cpy.at({0u});
		dfs_old.at({1u}) = df_cpy.at({1u});
		dfs_old.at({2u}) = df_cpy.at({2u});
		dfs_old.at({3u}) = df_cpy.at({4u});
		dfs_old.at({4u}) = df_cpy.at({3u});
		dfs_old.at({5u}) = df_cpy.at({7u});
		dfs_old.at({6u}) = df_cpy.at({8u});
		dfs_old.at({7u}) = df_cpy.at({5u});
		dfs_old.at({8u}) = df_cpy.at({6u});
	}
};

template<typename FP, typename Descriptor, typename Encode>
class component<FP, Descriptor, cmpt::Equilibrium, Encode> final {
private:
	saw::data<sch::Scalar<FP>> density_;
	saw::data<sch::Vector<FP,Descriptor::D>> momentum_;
public:
	component(
		saw::data<sch::Scalar<FP>> density__,
		saw::data<sch::Vector<FP,Descriptor::D>> momentum__
	):
		density_{density__},
		momentum_{momentum__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2u) == 0u);
		using dfi = df_info<FP,Descriptor>;
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto eq = equilibrium<FP,Descriptor>(density_,momentum_);

		dfs_old_f.at(index) = eq;
	}
};

/**
 * This is massively hacky and expects a lot of conditions
 * Either this or mirrored along the horizontal line works
 *
 * 0 - 2 - 2
 * 0 - 3 - 1
 * 0 - 3 - 1
 * .........
 * 0 - 3 - 1
 * 0 - 2 - 2
 *
 */
template<typename FP, bool Dir, typename Encode>
class component<FP, sch::Descriptor<2u,9u>, cmpt::ZouHePressureX<Dir>, Encode> final {
public:
	using Descriptor = sch::Descriptor<2u,9u>;
private:
	saw::data<FP> rho_setting_;
public:
	component(const saw::data<FP>& rho_setting__):
		rho_setting_{rho_setting__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64,2u>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<FP,Descriptor>;

		bool is_even = ((time_step.get() % 2) == 0);

		auto& info_f = field.template get<"info">();
		
		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();

		auto& dfs_old = dfs_old_f.at(index);

		auto rho_vel_x = [&]() -> saw::data<FP> {
			if constexpr (Dir){
				auto S = dfs_old.at({0u})
					+ dfs_old.at({3u}) + dfs_old.at({4u})
					+ (dfs_old.at({1u}) + dfs_old.at({5u}) + dfs_old.at({7u})) * 2u;
				return rho_setting_ - S;
			}
			else if constexpr (not Dir) {
				auto S = dfs_old.at({0u})
					+ dfs_old.at({3u}) + dfs_old.at({4u})
					+ (dfs_old.at({2u}) + dfs_old.at({6u}) + dfs_old.at({8u})) * 2u;
				return S - rho_setting_;
			}
			return {};
		}();

		// static_assert(Descriptor::D == 2u and Descriptor::Q == 9u, "Some parts are hard coded sadly");

		if constexpr (Dir) {
			dfs_old.at({2u}) = dfs_old.at({1u}) + saw::data<FP>{2.0 / 3.0} * rho_vel_x;
			dfs_old.at({6u}) = dfs_old.at({5u}) + saw::data<FP>{1.0 / 6.0} * rho_vel_x + saw::data<FP>{0.5} * (dfs_old.at({3u}) - dfs_old.at({4u}));
			dfs_old.at({8u}) = dfs_old.at({7u}) + saw::data<FP>{1.0 / 6.0} * rho_vel_x + saw::data<FP>{0.5} * (dfs_old.at({4u}) - dfs_old.at({3u}));
		}else if constexpr (not Dir){
			dfs_old.at({1u}) = dfs_old.at({2u}) - saw::data<FP>{2.0 / 3.0} * rho_vel_x;
			dfs_old.at({5u}) = dfs_old.at({6u}) - saw::data<FP>{1.0 / 6.0} * rho_vel_x + saw::data<FP>{0.5} * (dfs_old.at({4u}) - dfs_old.at({3u}));
			dfs_old.at({7u}) = dfs_old.at({8u}) - saw::data<FP>{1.0 / 6.0} * rho_vel_x + saw::data<FP>{0.5} * (dfs_old.at({3u}) - dfs_old.at({4u}));
		}
	}
};

template<typename FP, bool East, typename Encode>
class component<FP, sch::Descriptor<3u,27u>, cmpt::ZouHePressureX<East>, Encode> final {
public:
	using Descriptor = sch::Descriptor<3u,27u>;
private:
	saw::data<FP> rho_setting_;
public:
	component(const saw::data<FP>& rho_setting__):
		rho_setting_{rho_setting__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>> index, saw::data<sch::UInt64> time_step) const {
		using dfi = df_info<FP,Descriptor>;

		bool is_even = ((time_step.get() % 2) == 0);

		auto& info_f = field.template get<"info">();

		// auto& dfs_f = (is_even) ? field.template get<"dfs">() : field.template get<"dfs_old">();
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();
		    auto& f = dfs_old_f.at(index);

    /*
     * D3Q27 indexing
     *
     *             x=-1        x=0        x=+1
     *
     * z=+1:       19 22 25     18 21 24     20 23 26
     *
     * z= 0:        1  4  7      0  3  6      2  5  8
     *
     * z=-1:       10 13 16      9 12 15     11 14 17
     *
     * Boundary normal is x.
     *
     * East:
     *   known:   cx = +1
     *   unknown: cx = -1
     *
     * West:
     *   known:   cx = -1
     *   unknown: cx = +1
     */


    /*
     * ============================================================
     * rho * ux
     * ============================================================
     *
     * rho = S + 2 * sum(unknown)
     *
     * Therefore:
     *
     * East:
     *   rho*ux = rho - S
     *
     * West:
     *   rho*ux = S - rho
     */

    saw::data<FP> S;

    if constexpr (East) {

        S =
            // cx = 0
            f.at({0u})
            + f.at({3u})
            + f.at({6u})
            + f.at({9u})
            + f.at({12u})
            + f.at({15u})
            + f.at({18u})
            + f.at({21u})
            + f.at({24u})

            // cx = +1
            + saw::data<FP>{2.0} * (
                f.at({2u})
                + f.at({5u})
                + f.at({8u})
                + f.at({11u})
                + f.at({14u})
                + f.at({17u})
                + f.at({20u})
                + f.at({23u})
                + f.at({26u})
            );

    } else {

        S =
            // cx = 0
            f.at({0u})
            + f.at({3u})
            + f.at({6u})
            + f.at({9u})
            + f.at({12u})
            + f.at({15u})
            + f.at({18u})
            + f.at({21u})
            + f.at({24u})

            // cx = -1
            + saw::data<FP>{2.0} * (
                f.at({1u})
                + f.at({4u})
                + f.at({7u})
                + f.at({10u})
                + f.at({13u})
                + f.at({16u})
                + f.at({19u})
                + f.at({22u})
                + f.at({25u})
            );
    }

    saw::data<FP> rho_ux;

    if constexpr (East) {
        rho_ux = rho_setting_ - S;
    } else {
        rho_ux = S - rho_setting_;
    }


    /*
     * ============================================================
     * Tangential momentum: rho * uy
     * ============================================================
     *
     * For a pressure boundary, uy and uz are obtained from the
     * known populations.
     *
     * rho*uy:
     *
     * East:
     *
     *   x=0 contribution
     *   + 2*x=+1 contribution
     *
     * West:
     *
     *   x=0 contribution
     *   + 2*x=-1 contribution
     */

    saw::data<FP> rho_uy;
    saw::data<FP> rho_uz;

    if constexpr (East) {

        rho_uy =
            // cx = 0
            -f.at({3u})
            + f.at({6u})
            -f.at({12u})
            + f.at({15u})
            -f.at({21u})
            + f.at({24u})

            // cx = +1
            + saw::data<FP>{2.0} * (
                -f.at({5u})
                + f.at({8u})
                -f.at({14u})
                + f.at({17u})
                -f.at({23u})
                + f.at({26u})
            );

        rho_uz =
            // cx = 0
            -f.at({9u})
            -f.at({12u})
            -f.at({15u})
            + f.at({18u})
            + f.at({21u})
            + f.at({24u})

            // cx = +1
            + saw::data<FP>{2.0} * (
                -f.at({11u})
                -f.at({14u})
                -f.at({17u})
                +f.at({20u})
                +f.at({23u})
                +f.at({26u})
            );

    } else {

        rho_uy =
            // cx = 0
            -f.at({3u})
            + f.at({6u})
            - f.at({12u})
            + f.at({15u})
            - f.at({21u})
            + f.at({24u})

            // cx = -1
            + saw::data<FP>{2.0} * (
                -f.at({4u})
                + f.at({7u})
                -f.at({13u})
                + f.at({16u})
                -f.at({22u})
                + f.at({25u})
            );

        rho_uz =
            // cx = 0
            -f.at({9u})
            -f.at({12u})
            -f.at({15u})
            +f.at({18u})
            +f.at({21u})
            +f.at({24u})

            // cx = -1
            + saw::data<FP>{2.0} * (
                -f.at({10u})
                -f.at({13u})
                -f.at({16u})
                +f.at({19u})
                +f.at({22u})
                +f.at({25u})
            );
    }


    /*
     * ============================================================
     * Velocity
     * ============================================================
     */

    saw::data<FP> ux = rho_ux / rho_setting_;
    saw::data<FP> uy = rho_uy / rho_setting_;
    saw::data<FP> uz = rho_uz / rho_setting_;


    /*
     * ============================================================
     * Reconstruct unknown distributions
     * ============================================================
     *
     * Zou/He non-equilibrium bounce-back:
     *
     *   f_i = f_opp
     *       + (f_eq_i - f_eq_opp)
     *
     * For D3Q27:
     *
     * axis:
     *
     *   6*w*rho*u = 1/6 * rho*u
     *
     * face diagonal:
     *
     *   6*w*rho*(c.u)
     *       = 1/9 * rho*(c.u)
     *
     * body diagonal:
     *
     *   6*w*rho*(c.u)
     *       = 1/36 * rho*(c.u)
     *
     * The factors below therefore use:
     *
     *   1/6
     *   1/9
     *   1/36
     *
     * ============================================================
     */

    if constexpr (East) {

        /*
         * --------------------------------------------------------
         * (-1,  0,  0)  <-  (+1,  0,  0)
         * --------------------------------------------------------
         */

        f.at({1u}) =
            f.at({2u})
            - saw::data<FP>{1.0 / 6.0} * rho_ux;


        /*
         * --------------------------------------------------------
         * (-1, -1,  0)  <-  (+1, -1,  0)
         * --------------------------------------------------------
         */

        f.at({4u}) =
            f.at({5u})
            - saw::data<FP>{1.0 / 9.0}
                * (rho_ux + rho_uy);


        /*
         * --------------------------------------------------------
         * (-1, +1,  0)  <-  (+1, +1,  0)
         * --------------------------------------------------------
         */

        f.at({7u}) =
            f.at({8u})
            + saw::data<FP>{1.0 / 9.0}
                * (-rho_ux + rho_uy);


        /*
         * --------------------------------------------------------
         * (-1,  0, -1)  <-  (+1,  0, -1)
         * --------------------------------------------------------
         */

        f.at({10u}) =
            f.at({11u})
            - saw::data<FP>{1.0 / 9.0}
                * (rho_ux + rho_uz);


        /*
         * --------------------------------------------------------
         * (-1, -1, -1)  <-  (+1, -1, -1)
         * --------------------------------------------------------
         */

        f.at({13u}) =
            f.at({14u})
            - saw::data<FP>{1.0 / 36.0}
                * (rho_ux + rho_uy + rho_uz);


        /*
         * --------------------------------------------------------
         * (-1, +1, -1)  <-  (+1, +1, -1)
         * --------------------------------------------------------
         */

        f.at({16u}) =
            f.at({17u})
            + saw::data<FP>{1.0 / 36.0}
                * (-rho_ux + rho_uy + rho_uz);


        /*
         * --------------------------------------------------------
         * (-1,  0, +1)  <-  (+1,  0, +1)
         * --------------------------------------------------------
         */

        f.at({19u}) =
            f.at({20u})
            + saw::data<FP>{1.0 / 9.0}
                * (-rho_ux + rho_uz);


        /*
         * --------------------------------------------------------
         * (-1, -1, +1)  <-  (+1, -1, +1)
         * --------------------------------------------------------
         */

        f.at({22u}) =
            f.at({23u})
            + saw::data<FP>{1.0 / 36.0}
                * (-rho_ux - rho_uy + rho_uz);


        /*
         * --------------------------------------------------------
         * (-1, +1, +1)  <-  (+1, +1, +1)
         * --------------------------------------------------------
         */

        f.at({25u}) =
            f.at({26u})
            + saw::data<FP>{1.0 / 36.0}
                * (-rho_ux + rho_uy + rho_uz);

    } else {

        /*
         * --------------------------------------------------------
         * (+1,  0,  0)  <-  (-1,  0,  0)
         * --------------------------------------------------------
         */

        f.at({2u}) =
            f.at({1u})
            + saw::data<FP>{1.0 / 6.0} * rho_ux;


        /*
         * --------------------------------------------------------
         * (+1, -1,  0)  <-  (-1, -1,  0)
         * --------------------------------------------------------
         */

        f.at({5u}) =
            f.at({4u})
            + saw::data<FP>{1.0 / 9.0}
                * (rho_ux - rho_uy);


        /*
         * --------------------------------------------------------
         * (+1, +1,  0)  <-  (-1, +1,  0)
         * --------------------------------------------------------
         */

        f.at({8u}) =
            f.at({7u})
            + saw::data<FP>{1.0 / 9.0}
                * (rho_ux + rho_uy);


        /*
         * --------------------------------------------------------
         * (+1,  0, -1)  <-  (-1,  0, -1)
         * --------------------------------------------------------
         */

        f.at({11u}) =
            f.at({10u})
            + saw::data<FP>{1.0 / 9.0}
                * (rho_ux - rho_uz);


        /*
         * --------------------------------------------------------
         * (+1, -1, -1)  <-  (-1, -1, -1)
         * --------------------------------------------------------
         */

        f.at({14u}) =
            f.at({13u})
            + saw::data<FP>{1.0 / 36.0}
                * (rho_ux - rho_uy - rho_uz);


        /*
         * --------------------------------------------------------
         * (+1, +1, -1)  <-  (-1, +1, -1)
         * --------------------------------------------------------
         */

        f.at({17u}) =
            f.at({16u})
            + saw::data<FP>{1.0 / 36.0}
                * (rho_ux + rho_uy - rho_uz);


        /*
         * --------------------------------------------------------
         * (+1,  0, +1)  <-  (-1,  0, +1)
         * --------------------------------------------------------
         */

        f.at({20u}) =
            f.at({19u})
            + saw::data<FP>{1.0 / 9.0}
                * (rho_ux + rho_uz);


        /*
         * --------------------------------------------------------
         * (+1, -1, +1)  <-  (-1, -1, +1)
         * --------------------------------------------------------
         */

        f.at({23u}) =
            f.at({22u})
            + saw::data<FP>{1.0 / 36.0}
                * (rho_ux - rho_uy + rho_uz);


        /*
         * --------------------------------------------------------
         * (+1, +1, +1)  <-  (-1, +1, +1)
         * --------------------------------------------------------
         */

        f.at({26u}) =
            f.at({25u})
            + saw::data<FP>{1.0 / 36.0}
                * (rho_ux + rho_uy + rho_uz);
			}
		}
};


template<typename T, typename Descriptor, bool East, typename Encode>
class component<T, Descriptor, cmpt::ZouHeVelocityX<East>, Encode> final {
private:
	saw::data<sch::Vector<T,Descriptor::D>> velocity_;
public:
	component(
		const saw::data<sch::Vector<T,Descriptor::D>>& velocity__
	):
		velocity_{velocity__}
	{}

	template<typename CellFieldSchema>
	void apply(const saw::data<CellFieldSchema, Encode>& field, const saw::data<sch::FixedArray<sch::UInt64,Descriptor::D>>& index, saw::data<sch::UInt64> time_step) const {
		
		bool is_even = ((time_step.get() % 2u) == 0u);
		using dfi = df_info<T,Descriptor>;
		
		auto& dfs_old_f = (is_even) ? field.template get<"dfs_old">() : field.template get<"dfs">();


		auto& dfs = dfs_old_f.at(index);

		saw::data<T> dir_sum;
		if constexpr (East) {
			dir_sum = dfs.at({1u}) + dfs.at({5u}) + dfs.at({7u});
		} else if constexpr(not East){
			dir_sum = dfs.at({2u}) + dfs.at({6u}) + dfs.at({8u});
		}

		saw::data<sch::Scalar<T>> rho;
		rho.at({}) = (dfs.at({0u}) + dfs.at({4u}) + dfs.at({3u}) + dir_sum * 2) / (velocity_.at({{0u}}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(1.0)});

		if constexpr (East) {
			dfs.at({2u}) = dfs.at({1u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(2.0 / 3.0)} * rho.at({}) * velocity_.at({{0u}});
			dfs.at({6u}) = dfs.at({5u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(1.0 / 6.0)} * rho.at({}) * velocity_.at({{0u}})
				+ (dfs.at({3u}) - dfs.at({4u}) + rho.at({}) * velocity_.at({{1u}})) * 0.5f;
			dfs.at({8u}) = dfs.at({7u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(1.0 / 6.0)} * rho.at({}) * velocity_.at({{0u}})
				+ (dfs.at({4u}) - dfs.at({3u}) + rho.at({}) * velocity_.at({{1u}})) * 0.5f;
		}else if constexpr ( not East ){
			dfs.at({1u}) = dfs.at({2u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(2.0 / 3.0)} * rho.at({}) * velocity_.at({{0u}});
			dfs.at({5u}) = dfs.at({6u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(1.0 / 6.0)} * rho.at({}) * velocity_.at({{0u}})
				+ (dfs.at({4u}) - dfs.at({3u}) + rho.at({}) * velocity_.at({{1u}}))*0.5f;
			dfs.at({7u}) = dfs.at({8u}) + saw::data<T>{static_cast<saw::native_data_type<T>::type>(1.0 / 6.0)} * rho.at({}) * velocity_.at({{0u}})
				+ (dfs.at({3u}) - dfs.at({4u}) + rho.at({}) * velocity_.at({{1u}}))*0.5f;
		}
	}
};

}
}
