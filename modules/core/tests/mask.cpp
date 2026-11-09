#include <forstio/test/suite.hpp>

#include <cmath>
#include <iostream>

#include "../c++/particle/particle.hpp"
#include "../c++/particle/mask.hpp"

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

constexpr uint64_t D = 2u;

auto make_one(double radius){
	kel::lbm::data<sch::Scalar<sch::T>> r,d;
	r.at({}).set(radius);
	d.at({}).set(1.0);
	return kel::lbm::create_spheroid_particle_group<sch::T,D,1u>(r,d,{32u});
}

SAW_TEST("Mask / is actually filled"){
	using namespace kel;

	/**
	 * The voxelizer used to discard everything it computed, leaving every
	 * voxel at zero. A mask of all zeros reads as "surface everywhere", which
	 * is indistinguishable from a working mask until you look at a value.
	 */
	auto g = make_one(4.0);
	auto& mask = g.template get<"mask">();

	bool any_negative = false;
	bool any_positive = false;
	lbm::iterator<D>::apply([&](const auto& k){
		auto v = mask.at(k).get();
		if(v < -0.1){ any_negative = true; }
		if(v >  0.1){ any_positive = true; }
	},{},mask.get_dims());

	SAW_EXPECT(any_negative, "mask must have interior (negative) voxels");
	SAW_EXPECT(any_positive, "mask must have exterior (positive) voxels");
}

SAW_TEST("Mask / stores a signed distance"){
	using namespace kel;

	auto g = make_one(4.0);
	auto& mask = g.template get<"mask">();
	const uint64_t R = 32u;

	// Centre of a unit sphere mask: distance -1.
	kel::lbm::data<sch::Vector<sch::T,D>> centre;
	centre.at({{0u}}).set(0.0);
	centre.at({{1u}}).set(0.0);
	auto d_centre = (lbm::mask_sample_local<sch::T,D>)(mask,centre,R);
	SAW_EXPECT(std::abs(d_centre + 1.0) < 0.05,
		std::string{"centre distance should be -1: "} + std::to_string(d_centre));

	// On the surface along +x: distance 0.
	kel::lbm::data<sch::Vector<sch::T,D>> surf;
	surf.at({{0u}}).set(1.0);
	surf.at({{1u}}).set(0.0);
	auto d_surf = (lbm::mask_sample_local<sch::T,D>)(mask,surf,R);
	SAW_EXPECT(std::abs(d_surf) < 0.05,
		std::string{"surface distance should be 0: "} + std::to_string(d_surf));

	// Halfway out: distance -0.5.
	kel::lbm::data<sch::Vector<sch::T,D>> half;
	half.at({{0u}}).set(0.5);
	half.at({{1u}}).set(0.0);
	auto d_half = (lbm::mask_sample_local<sch::T,D>)(mask,half,R);
	SAW_EXPECT(std::abs(d_half + 0.5) < 0.05,
		std::string{"half radius distance should be -0.5: "} + std::to_string(d_half));
}

SAW_TEST("Mask / centre of mass is the body centre"){
	using namespace kel;

	// A sphere is symmetric, so its centroid must sit at the origin of the
	// local frame. The old code divided by a zero mass and produced 0/0.
	auto g = make_one(4.0);
	auto& com = g.template get<"center_of_mass">().at({0u});

	for(uint64_t i = 0u; i < D; ++i){
		SAW_EXPECT(std::isfinite(com.at({{i}}).get()), "com must be finite");
		SAW_EXPECT(std::abs(com.at({{i}}).get()) < 0.02,
			std::string{"com component "} + std::to_string(i) + ": "
			+ std::to_string(com.at({{i}}).get()));
	}
}

SAW_TEST("Mask / transform round trips"){
	using namespace kel;

	auto g = make_one(3.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(20.0);
	rb.template get<"position">().at({{1u}}).set(11.0);
	rb.template get<"rotation">().at({}).set(0.7);

	auto& com = g.template get<"center_of_mass">().at({0u});

	const double pts[4][2] = {{0.0,0.0},{0.5,-0.25},{-0.9,0.1},{1.0,0.0}};
	for(uint64_t c = 0u; c < 4u; ++c){
		kel::lbm::data<sch::Vector<sch::T,D>> u;
		u.at({{0u}}).set(pts[c][0]);
		u.at({{1u}}).set(pts[c][1]);

		auto x = (lbm::mask_local_to_global<sch::T,D>)(u,p,com);
		auto back = (lbm::mask_global_to_local<sch::T,D>)(x,p,com);

		for(uint64_t i = 0u; i < D; ++i){
			SAW_EXPECT(std::abs(back.at({{i}}).get() - u.at({{i}}).get()) < 1e-12,
				std::string{"round trip failed at point "} + std::to_string(c));
		}
	}
}

SAW_TEST("Mask / scale maps to physical radius"){
	using namespace kel;

	/**
	 * The mask is normalized, so the physical bounding radius is exactly the
	 * particle's radius. That identity is what lets the bounding-sphere broad
	 * phase and the voxel narrow phase agree by construction.
	 */
	auto g = make_one(3.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(10.0);
	rb.template get<"position">().at({{1u}}).set(10.0);
	rb.template get<"rotation">().at({}).set(0.0);

	auto& com = g.template get<"center_of_mass">().at({0u});
	const uint64_t R = 32u;

	// Centre: distance -radius.
	kel::lbm::data<sch::Vector<sch::T,D>> at_centre;
	at_centre.at({{0u}}).set(10.0);
	at_centre.at({{1u}}).set(10.0);
	auto d0 = (lbm::mask_signed_distance<sch::T,D>)(g,p,at_centre,R);
	SAW_EXPECT(std::abs(d0 + 3.0) < 0.2,
		std::string{"centre should read -radius: "} + std::to_string(d0));

	// Exactly one radius out: on the surface.
	kel::lbm::data<sch::Vector<sch::T,D>> at_surface;
	at_surface.at({{0u}}).set(13.0);
	at_surface.at({{1u}}).set(10.0);
	auto d1 = (lbm::mask_signed_distance<sch::T,D>)(g,p,at_surface,R);
	SAW_EXPECT(std::abs(d1) < 0.2,
		std::string{"one radius out should read 0: "} + std::to_string(d1));
}

SAW_TEST("Mask / solid fraction ramp"){
	using namespace kel;

	// Deep inside is fully solid, far outside fully fluid, and the ramp is
	// monotone across the shell rather than doubling back.
	SAW_EXPECT((lbm::solid_fraction_from_sdf<sch::T>)(-5.0, 1.5) == 1.0, "inside must be solid");
	SAW_EXPECT((lbm::solid_fraction_from_sdf<sch::T>)( 5.0, 1.5) == 0.0, "outside must be fluid");

	double prev = 1.0;
	for(int i = 0; i <= 20; ++i){
		double sdf = -0.75 + 1.5 * (i / 20.0);
		double f = (lbm::solid_fraction_from_sdf<sch::T>)(sdf, 1.5);
		SAW_EXPECT(f <= prev + 1e-12,
			std::string{"ramp must be monotone, broke at sdf="} + std::to_string(sdf));
		SAW_EXPECT(f >= -1e-12 && f <= 1.0 + 1e-12, "ramp must stay in [0,1]");
		prev = f;
	}
}

SAW_TEST("Mask / cube sdf is not a sphere"){
	using namespace kel;

	// The corner of a cube is further from the centre than its face, which is
	// what a sphere cannot express -- the point of keeping the mask generic.
	kel::lbm::data<sch::Vector<sch::T,D>> face;
	face.at({{0u}}).set(1.0);
	face.at({{1u}}).set(0.0);

	kel::lbm::data<sch::Vector<sch::T,D>> corner;
	corner.at({{0u}}).set(1.0);
	corner.at({{1u}}).set(1.0);

	SAW_EXPECT(std::abs((lbm::sdf::cube<sch::T,D>)(face)) < 1e-12, "face is on the surface");
	SAW_EXPECT(std::abs((lbm::sdf::cube<sch::T,D>)(corner)) < 1e-12, "corner is on the surface too");
	SAW_EXPECT((lbm::sdf::sphere<sch::T,D>)(corner) > 0.4, "but a sphere puts the corner outside");
}

SAW_TEST("Mask / porosity agrees with the analytic sphere"){
	using namespace kel;

	/**
	 * The mask path and the analytic path describe the same body, so they
	 * must agree to within the voxel pitch. This is the check that the whole
	 * "one field serves both consumers" idea rests on.
	 */
	auto g = make_one(5.0);
	auto& p = g.template get<"particles">().at({0u});
	auto& rb = p.template get<"rigid_body">();
	rb.template get<"position">().at({{0u}}).set(16.0);
	rb.template get<"position">().at({{1u}}).set(16.0);
	rb.template get<"rotation">().at({}).set(0.0);

	kel::lbm::data<sch::Scalar<sch::T>> eps;
	eps.at({}).set(1.5);
	auto rad = p.template get<"collision">().template get<"radius">();

	double worst = 0.0;
	for(int i = 0; i <= 40; ++i){
		double x = 16.0 + 10.0 * (i / 40.0) - 5.0;

		kel::lbm::data<sch::Vector<sch::T,D>> pos;
		pos.at({{0u}}).set(x);
		pos.at({{1u}}).set(16.0);

		kel::lbm::data<sch::Vector<sch::T,D>> rel;
		rel.at({{0u}}).set(x - 16.0);
		rel.at({{1u}}).set(0.0);

		auto analytic = (lbm::particle_porosity<sch::T,D,1u,lbm::por::ParticleSpheroid<sch::T>>::calculate)(rel,rad,eps);
		auto voxel = (lbm::particle_porosity<sch::T,D,1u,lbm::por::ParticleVoxel<sch::T>>::calculate)(g,0u,pos,eps);

		auto diff = std::abs(analytic.at({}).get() - voxel.at({}).get());
		if(diff > worst){ worst = diff; }
	}

	// Voxel pitch is 2*5/32 ~ 0.3 lattice units, and the ramp spans 1.5, so
	// the two profiles can differ by roughly a fifth of the ramp.
	SAW_EXPECT(worst < 0.35,
		std::string{"mask and analytic porosity disagree by "} + std::to_string(worst));
}
}
