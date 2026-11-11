#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>
#include <kel/lbm/particle.hpp>

#include <fstream>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>

namespace kel {
namespace lbm {

// Deliberately tiny: 32 x 32 x 32 = 32768 cells, so a full run finishes in
// seconds. Square in y/z so the channel cross section is symmetric about the
// particle, which makes a stray transverse force obvious.
constexpr uint64_t dim_x = 32ul;
constexpr uint64_t dim_y = 32ul;
constexpr uint64_t dim_z = 32ul;

constexpr uint64_t particle_amount = 1ul;

/**
 * The particle lives in lattice space, exactly like the fluid: the radius is a
 * number of cells, the density is relative to the fluid's rho = 1, and the
 * mass, inertia, force and torque that follow are all lattice quantities. The
 * converter handles the fluid's SI <-> LBM bookkeeping and is deliberately not
 * applied to any of this -- so every column of particle.csv is in cells,
 * steps, and lattice mass.
 */
constexpr double particle_radius = 0.15 * static_cast<double>(dim_y);
constexpr double particle_density = 1.0;

namespace sch {

/**
 * The spin axis is an arbitrary fixed vector, not a lattice direction. It is
 * normalised by the integrator, so these three need not be a unit vector.
 *
 * Defaults to +y: flow along x, axis vertical and perpendicular to it, blades
 * sweeping the x-z plane -- the vertical-axis turbine arrangement.
 */
using LbmArgs = Args<
	Struct<
		Member<String, "name">,
		Member<UInt64, "time_steps">,
		Member<UInt64, "sub_steps">,
		Member<Float64, "axis_x">,
		Member<Float64, "axis_y">,
		Member<Float64, "axis_z">
	>,
	Tuple<>
>;

using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y, dim_z>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y, dim_z>;

template<typename T, typename Desc>
using ScalarChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y, dim_z>;

template<typename T, typename Desc>
using VectorChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y, dim_z>;

template<typename T, typename Desc>
using ChunkStruct = Struct<
	Member<InfoChunk, "info">,
	Member<DfChunk<T,Desc>, "dfs">,
	Member<DfChunk<T,Desc>, "dfs_old">,
	Member<VectorChunk<T,Desc>, "particle_N">,
	Member<ScalarChunk<T,Desc>, "particle_D">
>;

template<typename T, typename Desc>
using MomChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y, dim_z>;

template<typename T>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y, dim_z>;

/**
 * "force" is not optional here: HlbmOneParticleGather reads it, so the 3D
 * macro struct needs it the moment a particle is carried.
 */
template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<MomChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">,
	Member<MomChunk<T,Desc>, "force">,
	Member<ScalarChunk<T,Desc>, "porosity">
>;

/**
 * particle_aabb is specialised only on ParticleGroup, so a bare
 * FixedArray<Particle<...>> hits its always_false primary template.
 */
template<typename T, typename Desc>
using ParticleSpheroidGroup = ParticleGroup<T,Desc::D,particle_amount,coll::Spheroid<T>>;
}

template<typename T, typename Desc>
kel::lbm::error_or<void> setup_initial_conditions(
		converter<T>& conv,
		kel::lbm::data<sch::ChunkStruct<T,Desc>>& fields,
		kel::lbm::data<sch::MacroStruct<T,Desc>>& macros,
		kel::lbm::data<sch::ParticleSpheroidGroup<T,Desc>>& particles
){
	// Unit conversion is not exercised in this reduced case.
	(void) conv;

	auto& info_f = fields.template get<"info">();
	// Set everything as walls
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(1u);
		},
		{},
		info_f.get_dims(),
		{}
	);
	// Fluid
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(2u);
		},
		{},
		info_f.get_dims(),
		{{1u,1u,1u}}
	);
	
	// Inflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(3u);
		},
		{{0u,0u,0u}},
		{{1u,dim_y,dim_z}},
		{{0u,1u,1u}}
	);
	
	// Outflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(4u);
		},
		{{dim_x-1u,0u,0u}},
		{{dim_x,dim_y,dim_z}},
		{{0u,1u,1u}}
	);
	//
	auto& df_f = fields.template get<"dfs_old">();
	auto& rho_f = macros.template get<"density">();
	auto& mom_f = macros.template get<"velocity">();
	auto& por_f = macros.template get<"porosity">();
	auto& force_f = macros.template get<"force">();

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			por_f.at(index).at({}) = {1};
			rho.at({}) = {1};
			auto& mom = mom_f.at(index);
			auto eq = equilibrium<T,Desc>(rho,mom);

			df = eq;

			auto& force = force_f.at(index);
			for(uint64_t i{0u}; i < Desc::D; ++i){
				force.at({{i}}).set(0);
			}
		},
		{},// 0-index
		df_f.get_dims()
	);

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			rho.at({}) = {1};
			auto& mom = mom_f.at(index);
			mom.at({{0u}}) = 0.01;
			auto eq = equilibrium<T,Desc>(rho,mom);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims(),
		{{1u,1u,1u}}
	);

	kel::lbm::data<sch::LbmMeter<sch::Scalar<T>>> rad;
	rad.at({}).set(particle_radius);
	kel::lbm::data<sch::Scalar<T>> dense;
	dense.at({}).set(particle_density);

	// Mask resolution 16 in 3D is 16^3 = 4096 voxels. The analytic spheroid
	// porosity is what HLBM actually calls, but the mask is still filled.
	particles = create_spheroid_particle_group<T,Desc::D,particle_amount>(rad,dense,{16u});
	{
		auto& rb = particles.template get<"particles">().at({0u}).template get<"rigid_body">();
		auto& rbp = rb.template get<"position">();
		rbp.at({{0u}}) = 0.3 * dim_x;
		rbp.at({{1u}}) = 0.5 * dim_y;
		rbp.at({{2u}}) = 0.5 * dim_z;

		// position_old == position means the body starts at rest. It also
		// makes a pinned body exact later: 2x - x_old + 0 == x.
		rb.template get<"position_old">() = rbp;

		kel::lbm::data<sch::Vector<T,Desc::D>> zero;
		for(uint64_t i{0u}; i < Desc::D; ++i){
			zero.at({{i}}).set(0);
		}
		// Acceleration is unit typed now, so its zero has to be too.
		kel::lbm::data<sch::LbmAcceleration<sch::Vector<T,Desc::D>>> zero_acc;
		for(uint64_t i{0u}; i < Desc::D; ++i){
			zero_acc.at({{i}}).set(0);
		}
		rb.template get<"acceleration">() = zero_acc;

		// In 3D these are Vector<T,3> (a rotation vector), not a scalar angle.
		rb.template get<"rotation">() = zero;
		rb.template get<"rotation_old">() = zero;
		rb.template get<"angular_acceleration">() = zero;
	}

	return kel::lbm::make_void();
}

template<typename T, typename Desc>
kel::lbm::error_or<void> step(
		converter<T>& conv,
		kel::lbm::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<kel::lbm::enc::Native>>& fields,
		kel::lbm::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<kel::lbm::enc::Native>>& macros,
		kel::lbm::data<sch::Ptr<sch::ParticleSpheroidGroup<T,Desc>>,encode::Sycl<kel::lbm::enc::Native>>& particles,
		kel::lbm::data<sch::UInt64> t_i,
		kel::lbm::data<sch::UInt64> sub_steps,
		const kel::lbm::data<sch::Vector<T,3u>>& spin_axis,
		device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();

	// Porosity back to 1 (all fluid) before the particle restamps itself.
	q.submit([&](acpp::sycl::handler& h){
		component<T,Desc,cmpt::HlbmReset,encode::Sycl<kel::lbm::enc::Native>> hlbm_reset;

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y,dim_z}, [=](acpp::sycl::id<Desc::D> idx){
			kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
			for(uint64_t i = 0u; i < Desc::D; ++i){
				index.at({{i}}).set(idx[i]);
			}

			hlbm_reset.apply(fields,macros,index,t_i);
		});
	}).wait();

	/**
	 * One work item per particle. The stamp has to come after the integrate:
	 * it writes the post-integration surface velocity back onto the lattice.
	 */
	q.submit([&](acpp::sycl::handler& h){
		component<T,Desc,cmpt::ParticleForceReset,encode::Sycl<kel::lbm::enc::Native>> p_reset;
		component<T,Desc,cmpt::HlbmOneParticleGather,encode::Sycl<kel::lbm::enc::Native>> p_gather;
		component<T,Desc,cmpt::HlbmOneParticleStamp,encode::Sycl<kel::lbm::enc::Native>> p_stamp;

		/**
		 * The rotor replaces the free-body integrator. No contact pass: the
		 * body is pinned, so it cannot reach a wall or another particle.
		 */
		component<T,Desc,cmpt::ParticleAxisIntegrate,encode::Sycl<kel::lbm::enc::Native>> p_integrate{spin_axis};

		h.parallel_for(acpp::sycl::range<1u>{particle_amount}, [=](acpp::sycl::id<1u> idx){
			kel::lbm::data<sch::FixedArray<sch::UInt64,1u>> index;
			for(uint64_t i = 0u; i < 1u; ++i){
				index.at({{i}}).set(idx[i]);
			}

			p_reset.apply(particles,index,t_i);
			p_gather.apply(fields,macros,particles,index,t_i,sub_steps);
			p_integrate.apply(particles,index,t_i,sub_steps);
			p_stamp.apply(fields,macros,particles,index,t_i,sub_steps);
		});
	}).wait();

	// auto coll_ev =
	q.submit([&](acpp::sycl::handler& h){
		/**
		 * Hlbm rather than BGK: it is the collision that consumes the
		 * porosity and particle_N/particle_D the stamp pass writes.
		 */
		component<T,Desc,cmpt::Hlbm, encode::Sycl<kel::lbm::enc::Native>> collision{conv.template kinematic_viscosity_si_to_tau<Desc>({0.1})};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<kel::lbm::enc::Native>> bb;

		component<T,Desc,cmpt::ZouHeHorizontal<true>,encode::Sycl<kel::lbm::enc::Native>> flow_in{
			[&](){
				uint64_t target_t_i = 256u;
				if(t_i.get() < target_t_i){
					return 1.0 + (0.001 / target_t_i) * t_i.get();
				}
				return 1.001;
			}()
		};
		component<T,Desc,cmpt::ZouHeHorizontal<false>,encode::Sycl<kel::lbm::enc::Native>> flow_out{1.0};

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y,dim_z}, [=](acpp::sycl::id<Desc::D> idx){
			kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
			for(uint64_t i = 0u; i < Desc::D; ++i){
				index.at({{i}}).set(idx[i]);
			}

			auto info = info_f.at(index);
			
			switch(info.get()){
				case 0u:
				break;
				case 1u:
					bb.apply(fields,index,t_i);
				break;
				case 2u:
					collision.apply(fields,macros,index,t_i);
				break;
				case 3u:
					flow_in.apply(fields,index,t_i);
					collision.apply(fields,macros,index,t_i);
				break;
				case 4u:
					flow_out.apply(fields,index,t_i);
					collision.apply(fields,macros,index,t_i);
				break;
				default:
				break;
			}
		});
	}).wait();

	q.submit([&](acpp::sycl::handler& h){
		component<T,Desc,cmpt::Stream,encode::Sycl<kel::lbm::enc::Native>> stream;

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y,dim_z}, [=](acpp::sycl::id<Desc::D> idx){
			kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
			for(uint64_t i = 0u; i < Desc::D; ++i){
				index.at({{i}}).set(idx[i]);
			}
			
			auto info = info_f.at(index);
			
			if(info.get() > 0u){
				stream.apply(fields,index,t_i);
			}
		});
	}).wait();

	// Step
	/*
	q.submit([&](acpp::sycl::handler& h){
		// h.depends_on(collision_ev);
	}).wait();
	*/

	return kel::lbm::make_void();
}
}
}

template<typename T, typename Desc>
kel::lbm::error_or<void> lbm_main(int argc, char** argv){
	using namespace kel::lbm;

	kel::lbm::data<sch::LbmArgs> args;
	{
		auto& args_n = args.template get<"args">();
		auto& name = args_n.template get<"name">();
		name.set("vawt_3d_hlbm_gpu");
		auto& steps_arg = args_n.template get<"time_steps">();
		steps_arg.set(256u);
		auto& sub_arg = args_n.template get<"sub_steps">();
		sub_arg.set(16u);

		// +y by default: flow along x, axis perpendicular to it.
		args_n.template get<"axis_x">().set(0.0);
		args_n.template get<"axis_y">().set(1.0);
		args_n.template get<"axis_z">().set(0.0);
	}
	{
		auto eov = kel::lbm::parse_args(args,argc,argv);
		if(eov.is_error()){
			return eov;
		}
	}
	auto eo_lbm_env = init_lbm_env(args);
	if(eo_lbm_env.is_error()){
		return std::move(eo_lbm_env.get_error());
	}
	const auto& lbm_env = eo_lbm_env.get_value();

	auto& out_dir = lbm_env.name_dir;

	{
		std::error_code ec;
		std::filesystem::create_directories(out_dir,ec);
		if(ec != std::errc{}){
			return kel::lbm::make_error<kel::lbm::err::critical>("Could not create output directory");
		}
	}

	converter<T> conv {
		// delta_x
		{1.0},
		// delta_t
		{1.0}
	};

	// Characteristic length is the body diameter -- the scale that sets Re here.
	print_lbm_meta<T,Desc>(conv,{0.1},{0.01},{2.0 * particle_radius});

	auto lbm_data_ptr = kel::lbm::heap<kel::lbm::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = kel::lbm::heap<kel::lbm::data<sch::MacroStruct<T,Desc>>>();
	auto lbm_particle_data_ptr = kel::lbm::heap<kel::lbm::data<sch::ParticleSpheroidGroup<T,Desc>>>();
	
	std::cout<<"Estimated Bytes: "<<memory_estimate<sch::ChunkStruct<T,Desc>,sch::MacroStruct<T,Desc>>().get()<<std::endl;

	auto eo_aio = saw::setup_async_io();
	if(eo_aio.is_error()){
		return kel::lbm::make_error<kel::lbm::err::critical>(
			std::string{eo_aio.get_error().get_message()});
	}
	auto& aio = eo_aio.get_value();
	saw::wait_scope wait{aio.event_loop};

	bool krun = true;
	bool print_status = false;
	aio.event_port.on_signal(saw::Signal::Terminate).then([&](){
		krun = false;
	}).detach();
	aio.event_port.on_signal(saw::Signal::User1).then([&](){
		print_status = true;
	}).detach();

	device dev;

	auto& sycl_q = dev.get_handle();

	sycl_q.wait();
	{
		auto eov = setup_initial_conditions<T,Desc>(conv,*lbm_data_ptr,*lbm_macro_data_ptr,*lbm_particle_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	kel::lbm::data<sch::ChunkStruct<T,Desc>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_data{sycl_q};
	kel::lbm::data<sch::MacroStruct<T,Desc>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_macro_data{sycl_q};
	kel::lbm::data<sch::ParticleSpheroidGroup<T,Desc>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_particle_data{sycl_q};
	sycl_q.wait();

	auto lsd_view = make_view(lbm_sycl_data);
	auto lsdm_view = make_view(lbm_sycl_macro_data);
	auto lsdp_view = make_view(lbm_sycl_particle_data);
	{
		auto eov = dev.copy_to_device(*lbm_data_ptr,lbm_sycl_data);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		auto eov = dev.copy_to_device(*lbm_macro_data_ptr,lbm_sycl_macro_data);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		auto eov = dev.copy_to_device(*lbm_particle_data_ptr,lbm_sycl_particle_data);
		if(eov.is_error()){
			return eov;
		}
	}
	sycl_q.wait();
	kel::lbm::data<sch::UInt64> time_steps = args.template get<"args">().template get<"time_steps">();
	kel::lbm::data<sch::UInt64> sub_steps = args.template get<"args">().template get<"sub_steps">();

	kel::lbm::data<sch::Vector<T,3u>> spin_axis;
	{
		auto& args_n = args.template get<"args">();
		spin_axis.at({{0u}}).set(args_n.template get<"axis_x">().get());
		spin_axis.at({{1u}}).set(args_n.template get<"axis_y">().get());
		spin_axis.at({{2u}}).set(args_n.template get<"axis_z">().get());
	}

	// Unit axis, for projecting the rotation and torque onto it below.
	auto axis_hat = kel::lbm::math::normalize(spin_axis);

	/**
	 * Particle time series. write_vtk_file only accepts a struct of chunks, so
	 * a particle group cannot go through it -- a CSV beside the VTK output is
	 * the simpler answer, and it is what you actually plot.
	 *
	 * Velocity and angular velocity are not stored on the body: the scheme is
	 * position-Verlet, so both are recovered as (now - previous) * steps.
	 */
	std::ofstream particle_csv{out_dir / "particle.csv"};
	if(not particle_csv.is_open()){
		return kel::lbm::make_error<kel::lbm::err::critical>("Could not open particle.csv");
	}
	/**
	 * theta/omega/alpha/tau are the rotation, angular velocity, angular
	 * acceleration and torque projected onto the spin axis -- the rotor
	 * numbers. drag is the streamwise force. The raw vectors follow so a
	 * stray off-axis component is still visible rather than hidden by the
	 * projection.
	 */
	particle_csv
		<<"step,theta,omega,alpha,tau,drag,"
		<<"x,y,z,vx,vy,vz,fx,fy,fz,"
		<<"rot_x,rot_y,rot_z,omega_x,omega_y,omega_z,"
		<<"alpha_x,alpha_y,alpha_z,tau_x,tau_y,tau_z\n";

	auto write_particle_row = [&](uint64_t step_i){
		auto& rb = lbm_particle_data_ptr->template get<"particles">()
			.at({0u}).template get<"rigid_body">();

		auto& pos     = rb.template get<"position">();
		auto& pos_old = rb.template get<"position_old">();
		auto& force   = rb.template get<"force">();
		auto& rot     = rb.template get<"rotation">();
		auto& rot_old = rb.template get<"rotation_old">();
		auto& ang_acc = rb.template get<"angular_acceleration">();
		auto& torque  = rb.template get<"torque">();

		const auto steps = static_cast<double>(particle_sub_steps(sub_steps));

		// Project onto the spin axis.
		double theta = 0.0, omega = 0.0, alpha = 0.0, tau = 0.0;
		for(uint64_t k{0u}; k < 3u; ++k){
			const double a = axis_hat.at({{k}}).get();
			theta += rot.at({{k}}).get() * a;
			omega += (rot.at({{k}}).get() - rot_old.at({{k}}).get()) * steps * a;
			alpha += ang_acc.at({{k}}).get() * a;
			tau   += torque.at({{k}}).get() * a;
		}

		particle_csv
			<<step_i
			<<","<<theta
			<<","<<omega
			<<","<<alpha
			<<","<<tau
			<<","<<force.at({{0u}}).get();

		particle_csv<<"";
		for(uint64_t k{0u}; k < Desc::D; ++k){
			particle_csv<<","<<pos.at({{k}}).get();
		}
		for(uint64_t k{0u}; k < Desc::D; ++k){
			particle_csv<<","<<(pos.at({{k}}).get() - pos_old.at({{k}}).get()) * steps;
		}
		for(uint64_t k{0u}; k < Desc::D; ++k){
			particle_csv<<","<<force.at({{k}}).get();
		}
		for(uint64_t k{0u}; k < 3u; ++k){
			particle_csv<<","<<rot.at({{k}}).get();
		}
		for(uint64_t k{0u}; k < 3u; ++k){
			particle_csv<<","<<(rot.at({{k}}).get() - rot_old.at({{k}}).get()) * steps;
		}
		for(uint64_t k{0u}; k < 3u; ++k){
			particle_csv<<","<<ang_acc.at({{k}}).get();
		}
		for(uint64_t k{0u}; k < 3u; ++k){
			particle_csv<<","<<torque.at({{k}}).get();
		}
		particle_csv<<"\n";
	};

	for(kel::lbm::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		{
			{
				auto eov = write_vtk_file(out_dir,"t",i.get(),*lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		{
			auto eov = step<T,Desc>(conv,lsd_view,lsdm_view,lsdp_view,i,sub_steps,spin_axis,dev);
			if(eov.is_error()){
				return eov;
			}
		}
		{
			auto eov = dev.copy_to_host(lbm_sycl_particle_data,*lbm_particle_data_ptr);
			if(eov.is_error()){
				return eov;
			}
		}
		write_particle_row(i.get());
		{
			auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
			if(eov.is_error()){
				return eov;
			}
		}
		{
			auto eov = dev.copy_to_host(lbm_sycl_data,*lbm_data_ptr);
			if(eov.is_error()){
				return eov;
			}
		}

		wait.poll();
		if(print_status){
			std::cout<<"Status: "<<i.get()<<" of "<<time_steps.get()<<" - "<<(i.template cast_to<sch::Float64>().get() * 100 / time_steps.get())<<"%"<<std::endl;
			print_status = false;
		}
	}
	sycl_q.wait();
	{
		auto eov = write_vtk_file(out_dir,"t",time_steps.get(),*lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	/*
	iterator<Desc::D>::apply(
		[&](auto& index){
			std::cout<<index.at({0u}).get()<<" "<<index.at({1u}).get()<<" "<<lbm_data.get<"info">().at(index).template cast_to<sch::UInt16>().get()<<"\n";
		},
		{{0u,0u}},
		{{dim_x, dim_y}}
	);
	*/

	sycl_q.wait();
	return kel::lbm::make_void();
}

using FloatT = kel::lbm::sch::Float32;

int main(int argc, char** argv){
	auto eov = lbm_main<FloatT,kel::lbm::sch::D3Q27>(argc, argv);
	if(eov.is_error()){
		auto& err = eov.get_error();
		std::cerr<<"[Error] "<<err.get_category();
		auto err_msg = err.get_message();
		if(err_msg.size() > 0u){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;
		return err.get_id();
	}
	return 0;
}
