#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>
#include <kel/lbm/particle.hpp>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_x = 1024ul;
constexpr uint64_t dim_y = 512ul;
constexpr uint64_t particle_size = 1ul;

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
	Member<DfChunk<T,Desc>, "dfs_old">,
	Member<VectorChunk<T,Desc>, "particle_N">,
	Member<ScalarChunk<T,Desc>, "particle_D">
>;

template<typename T, typename Desc>
using VelChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<VelChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">,
	Member<ScalarChunk<T,Desc>, "porosity">
>;

//template<typename T, typename Desc>
//using ParticleArray = Array<
//	Particle<T,Desc::D>
//>;
}

template<typename T, typename Desc>
saw::error_or<void> setup_initial_conditions(
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros,
		saw::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>>& particles
){
	auto& info_f = fields.template get<"info">();
	auto& porous_f = macros.template get<"porosity">();
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
	
	// Inflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(3u);
		},
		{{0u,0u}},
		{{1u,dim_y}},
		{{0u,1u}}
	);
	
	// Outflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(4u);
		},
		{{dim_x-1u,0u}},
		{{dim_x, dim_y}},
		{{0u,1u}}
	);
	//
	auto& df_f = fields.template get<"dfs_old">();
	auto& rho_f = macros.template get<"density">();
	auto& vel_f = macros.template get<"velocity">();
	auto& por_f = macros.template get<"porosity">();
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			por_f.at(index).at({}) = {1};
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims()
	);

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			vel.at({{0u}}) = 0.0;
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims(),
		{{1u,1u}}
	);
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			saw::data<sch::Vector<T,Desc::D>> middle, ind_vec;
			middle.at({{0u}}) = dim_x * 0.5;
			middle.at({{1u}}) = dim_y * 0.5;

			ind_vec.at({{0u}}) = index.at({{0u}}).template cast_to<T>();
			ind_vec.at({{1u}}) = index.at({{1u}}).template cast_to<T>();

			auto dist = middle - ind_vec;
			auto dist_2 = saw::math::dot(dist,dist);
			if(dist_2.at({}).get() < 128*128){
				porous_f.at(index).at({}) = 0.0;
			}
		},
		{},// 0-index
		df_f.get_dims()
	);

	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{particle_size}; ++i){
		auto& part = particles.at(i);

		saw::data<sch::Vector<T,Desc::D>> pos;
		pos.at({{0u}}) = dim_x * 0.5;
		pos.at({{1u}}) = dim_y * 0.5;
		saw::data<sch::Scalar<T>> rad, dense, dt;
		rad.at({}) = dim_y * 0.2;
		dense.at({}) = 1.0;
		dt.at({}) = 1.0;
		part = create_spheroid_particle(
			pos,{},{},
			{},{},{},
			rad, dense,dt
		);
	}

	return saw::make_void();
}

template<typename T, typename Desc>
saw::error_or<void> step(
		saw::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& fields,
		saw::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& macros,
		saw::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>>& particles,
		saw::data<sch::UInt64> t_i,
		device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();
	auto& porous_f = macros.template get<"porosity">();

	{
		auto& p = particles.at({{0u}});

		auto& p_coll = p.template get<"collision">();
		auto& p_rad = p_coll.template get<"radius">();
	}


	// auto coll_ev = 
	q.submit([&](acpp::sycl::handler& h){
		// Need nicer things to handle the flow. I see improvement here
		// component<T,Desc,cmpt::BGK, encode::Sycl<saw::encode::Native>> collision{0.6};
		// component<T,Desc,cmpt::HLBM,encode::Sycl<saw::encode::Native>> collision{0.6};
		component<T,Desc,cmpt::PSM,encode::Sycl<saw::encode::Native>> collision{0.6};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;

		saw::data<sch::Scalar<T>> rho_b;
		rho_b.at({}) = 1.0;
		saw::data<sch::Vector<T,Desc::D>> vel_b;
		vel_b.at({{0u}}) = 0.1;

		component<T,Desc,cmpt::Equilibrium,encode::Sycl<saw::encode::Native>> equi{rho_b,vel_b};

		component<T,Desc,cmpt::ZouHeHorizontal<true>,encode::Sycl<saw::encode::Native>> flow_in{
			[&](){
				uint64_t target_t_i = 256u;
				if(t_i.get() < target_t_i){
					return 1.0 + (0.001 / target_t_i) * t_i.get();
				}
				return 1.001;
			}()
		};
		component<T,Desc,cmpt::ZouHeHorizontal<false>,encode::Sycl<saw::encode::Native>> flow_out{1.0};


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
				case 3u:
					equi.apply(fields,index,t_i);
					// flow_in.apply(fields,index,t_i);
					collision.apply(fields,macros,index,t_i);
				break;
				case 4u:
					equi.apply(fields,index,t_i);
					// flow_out.apply(fields,index,t_i);
					collision.apply(fields,macros,index,t_i);
				break;
				default:
				break;
			}
		});
	}).wait();

	q.submit([&](acpp::sycl::handler& h){
		component<T,Desc,cmpt::Stream,encode::Sycl<saw::encode::Native>> stream;

		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
			saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
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

	return saw::make_void();
}
}
}

template<typename T, typename Desc>
saw::error_or<void> lbm_main(int argc, char** argv){
	using namespace kel::lbm;

	using dfi = df_info<T,Desc>;

	auto eo_lbm_dir = output_directory();
	if(eo_lbm_dir.is_error()){
		return std::move(eo_lbm_dir.get_error());
	}
	auto& lbm_dir = eo_lbm_dir.get_value();

	auto out_dir = lbm_dir / "poiseulle_particles_2d_gpu";

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
	
	print_lbm_meta<T,Desc>(conv,{0.01},{0.1},{1024.0});

	// saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	auto lbm_data_ptr = saw::heap<saw::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = saw::heap<saw::data<sch::MacroStruct<T,Desc>>>();
	auto lbm_particle_data_ptr = saw::heap<saw::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>>>();
	
	std::cout<<"Estimated Bytes: "<<memory_estimate<sch::ChunkStruct<T,Desc>,sch::MacroStruct<T,Desc>>().get()<<std::endl;

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

	saw::data<sch::ChunkStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_data{sycl_q};
	saw::data<sch::MacroStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_macro_data{sycl_q};
	saw::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>, encode::Sycl<saw::encode::Native>> lbm_sycl_particle_data{sycl_q};
	sycl_q.wait();

	auto lsd_view = make_chunk_struct_view(lbm_sycl_data);
	auto lsdm_view = make_chunk_struct_view(lbm_sycl_macro_data);
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
	saw::data<sch::UInt64> time_steps{8096ul};

	for(saw::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		{
			{
				auto eov = write_vtk_file(out_dir,"t",i.get(), *lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		{
			auto eov = step<T,Desc>(lsd_view,lsdm_view,*lbm_particle_data_ptr,i,dev);
			if(eov.is_error()){
				return eov;
			}
		}
		{
			auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
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
		auto eov = write_vtk_file(out_dir,"t",time_steps.get(), *lbm_macro_data_ptr);
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
