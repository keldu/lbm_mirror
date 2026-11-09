#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/integrate.hpp"
#include "../c++/particle/contact.hpp"
#include "../c++/particle/mask.hpp"
#include "../c++/particle/cell_list.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 3u;

/**
 * Nothing anywhere else in the suite instantiates D = 3, and templates are only
 * checked when instantiated -- which is how two "member does not exist" bugs
 * already slipped past the host tests and were only caught by a GPU build.
 * These exercise the 3D specializations so they cannot rot silently.
 */

template<uint64_t PC>
auto make_group(double radius, double density, uint64_t res = 12u){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(density);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,PC>(r,d,{res});
}

template<typename G>
void place(G& g, uint64_t i, double x, double y, double z){
	auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();
	auto& pos = rb.template get<"position">();
	auto& old = rb.template get<"position_old">();
	const double v[3] = {x,y,z};
	for(uint64_t i2 = 0u; i2 < 3u; ++i2){
		pos.at({{i2}}).set(v[i2]);
		// At rest. position_old defaults to zero, which would otherwise read
		// as an enormous separating velocity and damp the contact to nothing.
		old.at({{i2}}).set(v[i2]);
	}
}

SAW_TEST("3D / group construction fills mass and inertia"){
	using namespace kel;

	/**
	 * The 3D mass branch used to be empty, leaving total_mass at zero and then
	 * dividing the centre of mass by it.
	 */
	const double r = 2.0;
	const double rho = 1.5;
	auto g = make_group<2u>(r, rho);

	constexpr double pi = 3.14159265358979323846;
	const double expect_m = (4.0/3.0) * pi * r*r*r * rho;
	const double expect_i = 0.4 * expect_m * r * r;

	for(uint64_t i = 0u; i < 2u; ++i){
		auto& rb = g.template get<"particles">().at({i}).template get<"rigid_body">();

		auto m = rb.template get<"mass">().at({}).get();
		SAW_EXPECT(std::abs(m - expect_m) < 1e-9,
			std::string{"3D mass should be 4/3 pi r^3 rho: "} + std::to_string(m));

		auto in = rb.template get<"inertia">().at({}).get();
		SAW_EXPECT(std::abs(in - expect_i) < 1e-9,
			std::string{"3D inertia should be 2/5 m r^2: "} + std::to_string(in));
	}

	auto& com = g.template get<"center_of_mass">().at({0u});
	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(std::isfinite(com.at({{i}}).get()),
			"centre of mass must be finite, not 0/0");
		SAW_EXPECT(std::abs(com.at({{i}}).get()) < 0.05,
			std::string{"a sphere's centroid should sit at the origin, component "}
			+ std::to_string(i) + " = " + std::to_string(com.at({{i}}).get()));
	}
}

SAW_TEST("3D / mask is filled with a signed distance"){
	using namespace kel;

	auto g = make_group<1u>(2.0, 1.0, 16u);
	auto& mask = g.template get<"mask">();

	kel::lbm::data<sch::Vector<sch::T,D>> centre;
	for(uint64_t i = 0u; i < D; ++i){ centre.at({{i}}).set(0.0); }
	auto d_centre = (lbm::mask_sample_local<sch::T,D>)(mask,centre,16u);
	SAW_EXPECT(std::abs(d_centre + 1.0) < 0.12,
		std::string{"3D centre should read -1: "} + std::to_string(d_centre));

	kel::lbm::data<sch::Vector<sch::T,D>> surf;
	surf.at({{0u}}).set(1.0);
	surf.at({{1u}}).set(0.0);
	surf.at({{2u}}).set(0.0);
	auto d_surf = (lbm::mask_sample_local<sch::T,D>)(mask,surf,16u);
	SAW_EXPECT(std::abs(d_surf) < 0.12,
		std::string{"3D surface should read 0: "} + std::to_string(d_surf));
}

SAW_TEST("3D / Rodrigues rotation round trips"){
	using namespace kel;

	/**
	 * 3D rotation goes through Rodrigues on a rotation vector, a path with no
	 * other coverage at all.
	 */
	auto g = make_group<1u>(3.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();

	place(g, 0u, 20.0, 15.0, 10.0);

	// A rotation about a tilted axis, so no component is trivially preserved.
	auto& rot = rb.template get<"rotation">();
	rot.at({{0u}}).set(0.3);
	rot.at({{1u}}).set(-0.5);
	rot.at({{2u}}).set(0.9);

	auto& com = g.template get<"center_of_mass">().at({0u});

	const double pts[3][3] = {{1.0,0.0,0.0},{0.0,0.6,-0.3},{-0.4,0.2,0.8}};
	for(uint64_t c = 0u; c < 3u; ++c){
		kel::lbm::data<sch::Vector<sch::T,D>> u;
		for(uint64_t i = 0u; i < D; ++i){ u.at({{i}}).set(pts[c][i]); }

		auto x = (lbm::mask_local_to_global<sch::T,D>)(u,p,com);
		auto back = (lbm::mask_global_to_local<sch::T,D>)(x,p,com);

		for(uint64_t i = 0u; i < D; ++i){
			SAW_EXPECT(std::abs(back.at({{i}}).get() - u.at({{i}}).get()) < 1e-10,
				std::string{"3D round trip failed at point "} + std::to_string(c)
				+ " component " + std::to_string(i));
		}
	}
}

SAW_TEST("3D / zero rotation vector is the identity"){
	using namespace kel;

	// Rodrigues divides by the angle, so the zero vector is the degenerate
	// case that would produce NaN without a guard.
	auto g = make_group<1u>(2.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	place(g, 0u, 10.0, 10.0, 10.0);

	auto& rot = p.template get<"rigid_body">().template get<"rotation">();
	for(uint64_t i = 0u; i < D; ++i){ rot.at({{i}}).set(0.0); }

	auto& com = g.template get<"center_of_mass">().at({0u});

	kel::lbm::data<sch::Vector<sch::T,D>> u;
	u.at({{0u}}).set(1.0);
	u.at({{1u}}).set(0.0);
	u.at({{2u}}).set(0.0);

	auto x = (lbm::mask_local_to_global<sch::T,D>)(u,p,com);
	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(std::isfinite(x.at({{i}}).get()),
			"zero rotation must not produce NaN");
	}
	SAW_EXPECT(std::abs(x.at({{0u}}).get() - 12.0) < 0.05,
		std::string{"unrotated +x should sit at centre plus radius: "}
		+ std::to_string(x.at({{0u}}).get()));
}

SAW_TEST("3D / contact is antisymmetric and conserves momentum"){
	using namespace kel;

	auto g = make_group<2u>(2.0, 1.0);
	auto& parts = g.template get<"particles">();

	// Overlapping, offset on every axis so nothing cancels by symmetry.
	place(g, 0u, 10.0, 10.0, 10.0);
	place(g, 1u, 12.5, 10.8, 11.2);

	lbm::dem_params<sch::T> dem{8.0, 0.3, 2.0, 0.5};

	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::reset_particle_accumulators<sch::T,D>)(parts.at({i}));
	}
	for(uint64_t i = 0u; i < 2u; ++i){
		(lbm::accumulate_particle_contacts<sch::T,D>)(parts, i, dem, {1u});
	}

	for(uint64_t k = 0u; k < D; ++k){
		auto f0 = parts.at({0u}).template get<"rigid_body">()
			.template get<"force">().at({{k}}).get();
		auto f1 = parts.at({1u}).template get<"rigid_body">()
			.template get<"force">().at({{k}}).get();
		SAW_EXPECT(f0 == -f1,
			std::string{"3D force component "} + std::to_string(k)
			+ " not exactly opposite");
	}

	auto fx = parts.at({0u}).template get<"rigid_body">()
		.template get<"force">().at({{0u}}).get();
	SAW_EXPECT(std::abs(fx) > 0.0, "an overlapping 3D pair must feel a force");
}

SAW_TEST("3D / linear integration works and rotation stays gated"){
	using namespace kel;

	auto g = make_group<1u>(2.0, 1.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();

	place(g, 0u, 0.0, 0.0, 0.0);
	for(uint64_t i = 0u; i < D; ++i){
		rb.template get<"position_old">().at({{i}}).set(0.0);
	}

	auto mass = rb.template get<"mass">().at({}).get();
	rb.template get<"force">().at({{2u}}).set(mass);          // unit acceleration in z
	rb.template get<"torque">().at({{0u}}).set(100.0);        // and a large torque

	(lbm::integrate_particle<sch::T,D>)(p, {}, {1u});

	SAW_EXPECT(std::abs(rb.template get<"position">().at({{2u}}).get() - 1.0) < 1e-12,
		std::string{"3D linear motion should follow a dt^2: "}
		+ std::to_string(rb.template get<"position">().at({{2u}}).get()));

	/**
	 * 3D rotation is deliberately not integrated: a scalar inertia only
	 * describes an isotropic body, and integrating the rotation vector as a
	 * displacement cannot represent non-commuting composition. Assert the gate
	 * holds, so a future change cannot quietly start producing wrong spin.
	 */
	static_assert(!lbm::particle_angular_supported<3u>, "3D rotation must stay gated");
	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(rb.template get<"angular_acceleration">().at({{i}}).get() == 0.0,
			"3D angular acceleration must stay untouched even under torque");
	}
}

SAW_TEST("3D / cell list bins in three dimensions"){
	using namespace kel;

	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> extent{{24u,24u,24u}};
	auto cl = (lbm::make_cell_list<sch::T,D,8u>)(extent, 2.0);

	auto g = make_group<3u>(2.0, 1.0);
	auto& parts = g.template get<"particles">();
	place(g, 0u,  5.0,  5.0,  5.0);
	place(g, 1u,  6.0,  5.5,  5.0);
	place(g, 2u, 20.0, 20.0, 20.0);

	(lbm::build_cell_list<sch::T,D,8u>)(parts, cl);
	SAW_EXPECT(cl.template get<"overflow">().at({0u}).get() == 0u, "no overflow");

	// The 3^D neighbourhood is 27 bins here, not 9.
	kel::lbm::data<sch::FixedArray<sch::UInt64,D>> near{{5u,5u,5u}};
	bool saw[3] = {false,false,false};
	(lbm::for_each_bin_candidate<sch::T,D,8u>)(cl, near, [&](uint64_t p){ saw[p] = true; });

	SAW_EXPECT(saw[0] && saw[1], "neighbours must be found in 3D");
	SAW_EXPECT(!saw[2], "the distant particle must be pruned in 3D");
}
}
