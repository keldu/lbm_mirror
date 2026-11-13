#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>
#include <kel/lbm/particle.hpp>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>

namespace kel {
namespace lbm {

// The channel this case has always used.
constexpr uint64_t dim_x = 2048ul;
constexpr uint64_t dim_y = 128ul;
constexpr uint64_t dim_z = 128ul;
constexpr uint64_t particle_size = 4ul;

namespace sch {

using LbmArgs = Args<
	Struct<
		Member<String, "name">,
		Member<UInt64, "time_steps">
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

template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<MomChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">,
	Member<ScalarChunk<T,Desc>, "porosity">
>;
}

template<typename T, typename Desc>
kel::lbm::error_or<void> setup_initial_conditions(
		converter<T>& conv,
		kel::lbm::data<sch::ChunkStruct<T,Desc>>& fields,
		kel::lbm::data<sch::MacroStruct<T,Desc>>& macros,
		kel::lbm::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>>& particles
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
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			por_f.at(index).at({}) = {1};
			rho.at({}) = {1};
			auto& mom = mom_f.at(index);
			auto eq = equilibrium<T,Desc>(rho,mom);

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
			auto& mom = mom_f.at(index);
			mom.at({{0u}}) = 0.01;
			auto eq = equilibrium<T,Desc>(rho,mom);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims(),
		{{1u,1u,1u}}
	);

	// No particles in this case -- the array is carried only so the schema
	// matches poiseulle_3d_gpu.
	(void) particles;

	return kel::lbm::make_void();
}

template<typename T, typename Desc>
kel::lbm::error_or<void> step(
		converter<T>& conv,
		kel::lbm::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<kel::lbm::enc::Native>>& fields,
		kel::lbm::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<kel::lbm::enc::Native>>& macros,
		kel::lbm::data<sch::UInt64> t_i,
		device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();

	// auto coll_ev = 
	q.submit([&](acpp::sycl::handler& h){
		// Need nicer things to handle the flow. I see improvement here
		component<T,Desc,cmpt::BGK, encode::Sycl<kel::lbm::enc::Native>> collision{conv.template kinematic_viscosity_si_to_tau<Desc>({0.1})};
		// component<T,Desc,cmpt::HLBM,encode::Sycl<kel::lbm::enc::Native>> collision{0.6};
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
		name.set("poiseulle_3d");
		auto& steps_arg = args_n.template get<"time_steps">();
		steps_arg.set(64u);
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

	print_lbm_meta<T,Desc>(conv,{0.1},{0.01},{1024.0});

	auto lbm_data_ptr = kel::lbm::heap<kel::lbm::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = kel::lbm::heap<kel::lbm::data<sch::MacroStruct<T,Desc>>>();
	auto lbm_particle_data_ptr = kel::lbm::heap<kel::lbm::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>>>();
	
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
	kel::lbm::data<sch::FixedArray<sch::Particle<T,Desc::D>, particle_size>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_particle_data{sycl_q};
	sycl_q.wait();

	auto lsd_view = make_view(lbm_sycl_data);
	auto lsdm_view = make_view(lbm_sycl_macro_data);
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

	for(kel::lbm::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		{
			{
				auto eov = write_vtk_file(out_dir,"t",i.get(),*lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
			/*{
				std::string file_name = "p_";
				file_name += std::to_string(i.get());
				file_name += ".json";
				auto eov = saw::easy::encode_and_write_file<sch::FixedArray<sch::Particle<T,Desc::D>,particle_size>,saw::encode::Json>(out_dir, *lbm_particle_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}*/
		}
		{
			auto eov = step<T,Desc>(conv,lsd_view,lsdm_view,i,dev);
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
