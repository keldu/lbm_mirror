#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>
#include <kel/lbm/particle.hpp>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>
#include <forstio/codec/math.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_x = 512ul;
constexpr uint64_t dim_y = dim_x * 2ul;

constexpr uint64_t particle_amount = 16ul;

namespace sch {
using namespace saw::schema;

using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y>;

template<typename T, typename Desc>
using ScalarChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using VectorChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using ChunkStruct = Struct<
	Member<InfoChunk, "info">,
	Member<DfChunk<T,Desc>, "dfs">,
	Member<DfChunk<T,Desc>, "dfs_old">
>;

template<typename T, typename Desc>
using VelChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<VelChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">,
	Member<VectorChunk<T,Desc>, "force">
>;

template<typename T, typename Desc>
using ParticleSpheroidGroup = ParticleGroup<
	T,
	Desc::D,
	sch::ParticleCollisionSpheroid<T,2.0f>
>;

template<typename T, typename Desc>
using ParticleGroups = Tuple<
	ParticleSpheroidGroup<T,Desc>
>;


}

template<typename T, typename Desc>
saw::error_or<void> setup_initial_conditions(
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros,
		saw::data<sch::ParticleSpheroidGroup<T,Desc>>& particles
){
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
		{{1u,1u}}
	);
	//
	auto& df_f = fields.template get<"dfs_old">();
	auto& rho_f = macros.template get<"density">();
	auto& vel_f = macros.template get<"velocity">();
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims()
	);

	// Particles
	{
		saw::data<sch::Scalar<T>> dense_p;
		dense_p.at({}).set(1);
		// auto& spheroid_group = particles.template get<0u>();
		auto& spheroid_group = particles;

		spheroid_group = create_spheroid_particle_group<T,Desc::D,2.0f>(
			dense_p,
			{64u}
		);

		{
			auto& p = spheroid_group.template get<"particles">();

			p = {{{16u}}};

			iterator<1u>::apply(
				[&](auto& index){
					// Set Pos here?
					auto& p_ind = p.at(index);

					auto& p_rb = p_ind.template get<"rigid_body">();
					auto& p_pos = p_rb.template get<"position">();

					// TODO CONTINUE HERE NEED to init pos here !!!!

					auto& p_pos_old = p_rb.template get<"position_old">();
					p_pos_old = p_pos;
				},
				{},
				p.meta()
			);
		}
	}
	// Particle in hacky flavour
	{
	}

	return saw::make_void();
}

template<typename T, typename Desc>
saw::error_or<void> step(
	saw::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& fields,
	saw::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& macros,
	saw::data<sch::Ptr<sch::ParticleSpheroidGroup<T,Desc>>,encode::Sycl<saw::encode::Native>>& p_group,
	saw::data<sch::UInt64> t_i,
	device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();

	auto& parts = p_group.template get<"particles">();
	auto& p_mask = p_group.template get<"mask">();
	auto& vels = macros.template get<"velocity">();
	auto& forces = macros.template get<"force">();

	auto p_meta = parts.meta();
	q.submit([&](acpp::sycl::handler& h){

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
			for(uint64_t i = 0u; i < Desc::D; ++i){
				index.at({{i}}).set(idx[i]);
			}

			// Reset the force to zero
			forces.at(index) = {};
		});
	}).wait();

	q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<1u>{p_meta.at({0u}).get()}, [=](acpp::sycl::id<1u> idx){

			saw::data<sch::FixedArray<sch::UInt64,1u>> index;
			for(uint64_t i = 0u; i < 1u; ++i){
				index.at({{i}}).set(idx[i]);
			}

			auto& p = parts.at(index);
			auto& p_rb = p.template get<"rigid_body">();
			saw::data<sch::Scalar<T>> delta_t;
			delta_t.at({}).set(1.0f);

			auto& p_pos = p_rb.template get<"position">();
			auto& p_rot = p_rb.template get<"rotation">();

			iterator<Desc::D>::apply(
				[&](auto& m_ind){
					saw::data<sch::Vector<T,Desc::D>> index_shift;
					for(uint64_t i{0u}; i < Desc::D; ++i){
						index_shift.at({{i}}) = m_ind.at({i}).template cast_to<T>() - (p_mask.meta().at({i})+1u).template cast_to<T>() * 0.5;
					}

					saw::data<sch::Vector<T,Desc::D>> transformed_pos;
					for(uint64_t i{0u}; i < Desc::D; ++i){
						// TODO add rotation, scaling here.
						transformed_pos.at({{i}}) = index_shift.at({{i}});
					}

					// Lagrange indicator position
					auto p_pos_lag = p_pos + transformed_pos;

					// Pick the closest velocity
					saw::data<sch::FixedArray<sch::UInt64,Desc::D>> p_cell_pos;
					saw::data<sch::Vector<sch::UInt64,Desc::D>> p_cell_pos_vec;
					for(uint64_t i{0u}; i < Desc::D; ++i){
						p_cell_pos.at({{i}}) = (p_pos_lag.at({{i}}) + 0.5).template cast_to<sch::UInt64>();
						p_cell_pos.at({{i}}).set(std::max(1ul,std::min(p_cell_pos.at({{i}}).get(), p_meta.at({{i}}).get() - 2ul)));
						p_cell_pos_vec.at({{i}}) = p_cell_pos.at({{i}});
					}

					auto& u_fluid = vels.at(p_cell_pos);

					// this is our relative position to the particle
					auto rel_cell_to_part_pos = p_cell_pos_vec.template cast_to<T>() - p_pos;

					auto p_vel = (p_pos - p_rb.template get<"position_old">()) * delta_t;
					auto u_solid = p_vel + saw::math::cross(p_rot,rel_cell_to_part_pos);


					// Force
					auto force = (u_solid - u_fluid) / delta_t;

					// TODO HERE ATOMIC! !!!!
					forces.at(p_cell_pos) = forces.at(p_cell_pos) + force;
				},
				{},
				p_mask.meta()
			);

			verlet_step_lambda<T,Desc::D>(p,delta_t);
		});
	}).wait();

	// auto coll_ev = 
	q.submit([&](acpp::sycl::handler& h){
		// Need nicer things to handle the flow. I see improvement here
		component<T,Desc,cmpt::BGKGuo, encode::Sycl<saw::encode::Native>> collision{0.8};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
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
				default:
				break;
			}

		});
	}).wait();

	// Step
	/*
	q.submit([&](acpp::sycl::handler& h){
		// h.depends_on(collision_ev);
	}).wait();
	*/

	return saw::make_void();
}
}
}

template<typename T, typename Desc>
saw::error_or<void> lbm_main(int argc, char** argv){
	using namespace kel::lbm;

	using dfi = df_info<T,Desc>;

	auto eo_lbm_env = lbm_directory();
	if(eo_lbm_env.is_error()){
		return std::move(eo_lbm_env.get_error());
	}
	auto& lbm_env = eo_lbm_env.get_value();

	auto out_dir = lbm_env.data_dir / "settling_cubes_2d_ibm_gpu";

	{
		std::error_code ec;
		std::filesystem::create_directories(out_dir,ec);
		if(ec != std::errc{}){
			return saw::make_error<saw::err::critical>("Could not create output directory");
		}
	}

	converter<T> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};
	
	print_lbm_meta<T,Desc>(conv,{0.05},{0.01},{0.4 * dim_y});

	// saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	auto lbm_data_ptr = saw::heap<saw::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = saw::heap<saw::data<sch::MacroStruct<T,Desc>>>();
	auto lbm_particle_data_ptr = saw::heap<saw::data<sch::ParticleSpheroidGroup<T,Desc>>>();
	// auto lbm_particles_ptr = saw::heap<saw::data<sch::FixedArray<sch::ParticleRigidBody<T,Desc::D>,part_count>>>();
	// saw::data<sch::Array<T,Desc::D>> p_mask;

	std::cout<<"Estimated Bytes of LBM Fields: "<<memory_estimate<sch::ChunkStruct<T,Desc>,sch::MacroStruct<T,Desc>>().get()<<std::endl;

	auto eo_aio = saw::setup_async_io();
	if(eo_aio.is_error()){
		return std::move(eo_aio.get_error());
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
		auto eov = setup_initial_conditions<T,Desc>(*lbm_data_ptr,*lbm_macro_data_ptr,*lbm_particle_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		auto eov = write_vtk_file(out_dir,"initial_state",0u,*lbm_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	saw::data<sch::ChunkStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_data{sycl_q};
	saw::data<sch::MacroStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_macro_data{sycl_q};
	saw::data<sch::ParticleSpheroidGroup<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_particle_data{sycl_q};

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

	auto lsd_view = make_view(lbm_sycl_data);
	auto lsdm_view = make_view(lbm_sycl_macro_data);
	auto lsdp_view = make_view(lbm_sycl_particle_data);

	saw::data<sch::UInt64> time_steps{16u*4096ul};

	auto& info_f = lsd_view.template get<"info">();

	for(saw::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		// BC + Collision
		{
			auto eov = step<T,Desc>(lsd_view,lsdm_view,lsdp_view,i,dev);
			if(eov.is_error()){
				return eov;
			}
		}
		sycl_q.wait();
		if(i.get()%32u ==0u){
			{
				auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
			{
				auto eov = write_vtk_file(out_dir,"m",i.get(), *lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		/*
		{
			{
				auto eov = dev.copy_to_host(lbm_sycl_data,*lbm_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
			{
				auto eov = write_csv_file(out_dir,"lbm",i.get(), *lbm_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		*/
		// Stream
		sycl_q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::Stream,encode::Sycl<saw::encode::Native>> stream;

			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
				for(uint64_t i = 0u; i < Desc::D; ++i){
					index.at({{i}}).set(idx[i]);
				}
				
				auto info = info_f.at(index);
				
				if(info.get() > 0u){
					stream.apply(lsd_view,index,i);
				}
			});
		}).wait();
		wait.poll();
		if(print_status){
			std::cout<<"Status: "<<i.get()<<" of "<<time_steps.get()<<" - "<<(i.template cast_to<sch::Float64>().get() * 100 / time_steps.get())<<"%"<<std::endl;
			print_status = false;
		}
		print_progress_bar(i.get(), time_steps.get()-1u);
	}

	// After Loop
	sycl_q.wait();
	{
		auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		auto eov = write_vtk_file(out_dir,"m",time_steps.get(), *lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	sycl_q.wait();
	return saw::make_void();
}

using FloatT = kel::lbm::sch::Float32;

int main(int argc, char** argv){
	auto eov = lbm_main<FloatT,kel::lbm::sch::D2Q9>(argc, argv);
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
