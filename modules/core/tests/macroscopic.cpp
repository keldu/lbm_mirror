#include <forstio/test/suite.hpp>

#include <cmath>

#include "../c++/macroscopic.hpp"
#include "../c++/equilibrium.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

/**
 * The macro field used to be called "momentum" while holding velocity:
 * compute_rho_u divides the first moment by the density (macroscopic.hpp).
 * These pin that the name now matches the content, so the rename cannot be
 * quietly undone by removing the division.
 */

template<typename Desc>
void roundtrip(double rho_in, double ux_in){
	using namespace kel;

	lbm::data<sch::Scalar<sch::T>> rho;
	rho.at({}).set(rho_in);

	lbm::data<sch::Vector<sch::T,Desc::D>> vel;
	for(uint64_t i = 0u; i < Desc::D; ++i){
		vel.at({{i}}).set(0.0);
	}
	vel.at({{0u}}).set(ux_in);

	auto dfs = lbm::equilibrium<sch::T,Desc>(rho,vel);

	lbm::data<sch::Scalar<sch::T>> rho_out;
	lbm::data<sch::Vector<sch::T,Desc::D>> vel_out;
	lbm::compute_rho_u<sch::T,Desc>(dfs,rho_out,vel_out);

	SAW_EXPECT(std::abs(rho_out.at({}).get() - rho_in) < 1e-12,
		"density must round trip through equilibrium");
	SAW_EXPECT(std::abs(vel_out.at({{0u}}).get() - ux_in) < 1e-12,
		"velocity must round trip through equilibrium");
}

SAW_TEST("Macroscopic / the field holds velocity, not momentum"){
	using namespace kel;
	using Desc = sch::D2Q9;

	// Density deliberately not 1, which is the only case where velocity and
	// momentum differ numerically.
	const double rho_in = 2.0;
	const double ux_in = 0.1;

	lbm::data<sch::Scalar<sch::T>> rho;
	rho.at({}).set(rho_in);
	lbm::data<sch::Vector<sch::T,Desc::D>> vel;
	vel.at({{0u}}).set(ux_in);
	vel.at({{1u}}).set(0.0);

	auto dfs = lbm::equilibrium<sch::T,Desc>(rho,vel);

	lbm::data<sch::Scalar<sch::T>> rho_out;
	lbm::data<sch::Vector<sch::T,Desc::D>> vel_out;
	lbm::compute_rho_u<sch::T,Desc>(dfs,rho_out,vel_out);

	const double got = vel_out.at({{0u}}).get();

	SAW_EXPECT(std::abs(got - ux_in) < 1e-12, "must be the velocity u");
	// rho*u would be 0.2 here. If the division by rho were dropped, this is
	// the assertion that would catch it.
	SAW_EXPECT(std::abs(got - rho_in * ux_in) > 1e-3,
		"must NOT be the momentum rho*u");
}

SAW_TEST("Macroscopic / round trip D2Q9"){
	roundtrip<sch::D2Q9>(1.0, 0.05);
	roundtrip<sch::D2Q9>(2.0, 0.05);
}

SAW_TEST("Macroscopic / round trip D2Q5"){
	roundtrip<sch::Descriptor<2,5>>(1.0, 0.0);
}

SAW_TEST("Macroscopic / round trip D3Q27"){
	// The 3D path divides by rho too -- it is the same loop.
	roundtrip<sch::D3Q27>(1.0, 0.05);
	roundtrip<sch::D3Q27>(1.7, 0.03);
}

SAW_TEST("Macroscopic / a quiescent field has zero velocity at any density"){
	using namespace kel;
	using Desc = sch::D2Q9;

	for(double r : {0.5, 1.0, 3.0}){
		lbm::data<sch::Scalar<sch::T>> rho;
		rho.at({}).set(r);
		lbm::data<sch::Vector<sch::T,Desc::D>> vel;
		vel.at({{0u}}).set(0.0);
		vel.at({{1u}}).set(0.0);

		auto dfs = lbm::equilibrium<sch::T,Desc>(rho,vel);

		lbm::data<sch::Scalar<sch::T>> rho_out;
		lbm::data<sch::Vector<sch::T,Desc::D>> vel_out;
		lbm::compute_rho_u<sch::T,Desc>(dfs,rho_out,vel_out);

		SAW_EXPECT(std::abs(rho_out.at({}).get() - r) < 1e-12, "density preserved");
		SAW_EXPECT(std::abs(vel_out.at({{0u}}).get()) < 1e-12, "no x velocity at rest");
		SAW_EXPECT(std::abs(vel_out.at({{1u}}).get()) < 1e-12, "no y velocity at rest");
	}
}
}
