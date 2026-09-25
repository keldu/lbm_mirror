#include "common.hpp"
#include "init.hpp"
#include "step.hpp"

#include <forstio/codec/args.hpp>

/**
 * For deciding what parameters to use maybe?
 */
namespace args {
using namespace saw::schema;

using LbmArgsStruct = Struct<
	Member<String, "coupling">
>;

using LbmArgs = Args<
	LbmArgsStruct,
	Tuple<>
>;
}

template<typename T, typename Desc, typename Coll>
saw::error_or<void> lbm_main(const saw::data<args::LbmArgs>& args){
	using namespace kel::lbm;

	auto& an = args.template get<"args">();

	using dfi = df_info<T,Desc>;

	auto eo_lbm_dir = output_directory();
	if(eo_lbm_dir.is_error()){
		return std::move(eo_lbm_dir.get_error());
	}
	auto& lbm_dir = eo_lbm_dir.get_value();

	auto out_dir = lbm_dir / "ops_2d" / an.template get<"coupling">().stl_view();

	{
		std::error_code ec;
		std::filesystem::create_directories(out_dir,ec);
		if(ec != std::errc{}){
			return saw::make_error<saw::err::critical>("Could not create output directory");
		}
	}

	converter<T> conv {
		// delta_x
		{{0.02 / dim_x}},
		// delta_t
		{{3e-7}}
	};
	saw::data<sch::SiKinematicViscosity<T>> kin_vis_si{1e-3};
	auto kin_vis_lbm = conv.kinematic_viscosity_si_to_lbm(kin_vis_si);
	auto tau = conv.template kinematic_viscosity_lbm_to_tau<Desc>(kin_vis_lbm);
	
	print_lbm_meta<T,Desc>(conv,kin_vis_si,{1e-4},{2});
	std::cout<<"Position: "<<conv.meter_si_to_lbm({{0.01}}).handle().get()<<", "<<conv.meter_si_to_lbm({{0.04}}).handle().get()<<std::endl;

	// saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	auto lbm_data_ptr = saw::heap<saw::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = saw::heap<saw::data<sch::MacroStruct<T,Desc>>>();
	auto lbm_particle_data_ptr = saw::heap<saw::data<sch::ParticleSpheroidGroup<T,Desc>>>();

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
		auto eov = init<T,Desc,Coll>(conv,*lbm_data_ptr,*lbm_macro_data_ptr,*lbm_particle_data_ptr);
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
	sycl_q.wait();

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
	
	saw::data<sch::UInt64> time_steps{1024ul * 1024ul * 64ul};

	auto& info_f = lsd_view.template get<"info">();

	for(saw::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		// BC + Collision
		{
			auto eov = step<T,Desc,Coll>(
				conv, 
				tau, 
				lsd_view, 
				lsdm_view, 
				lsdp_view, 
				i, 
				dev
			);
			if(eov.is_error()){
				return eov;
			}
		}

		sycl_q.wait();
		if(i.get() % 1024u == 0u){
			{
				auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
			{
				auto eov = write_vtk_file(out_dir,"ops_2d",i.get(), *lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		// Stream
		sycl_q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::Stream,encode::Sycl<saw::encode::Native>> stream;

			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
				for(uint64_t i = 0u; i < Desc::D; ++i){
					index.at({{i}}).set(idx[i]);
				}

				auto info = info_f.at(index).get();

				if(info > 0u){
					stream.apply(lsd_view,index,i);
				}
			});
		}).wait();
		wait.poll();

		if(print_status){
			std::cout<<"Status: "<<i.get()<<" of "<<time_steps.get()<<" - "<<(i.template cast_to<sch::Float64>().get() * 100 / time_steps.get())<<"%"<<std::endl;
			print_status = false;
		}
		print_progress_bar(i.get()+1u, time_steps.get());
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
		auto eov = write_vtk_file(out_dir,std::string{"ops_2d_"}+an.template get<"coupling">().stl_string(),time_steps.get(), *lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	sycl_q.wait();
	return saw::make_void();
}

using FloatT = kel::lbm::sch::Float32;
using DescT = kel::lbm::sch::D2Q9;

saw::error_or<void> k_main(int argc, char** argv){
	using namespace kel::lbm;
	
	saw::data<args::LbmArgs> args;
	{
		auto eov = saw::parse_args(args,argc,argv);
		if(eov.is_error()){
			return eov;
		}
	}
	auto& an = args.template get<"args">();

	auto& coupling = an.template get<"coupling">();

	if(coupling.stl_view() == "hlbm"){
		return lbm_main<FloatT,DescT,method::Hlbm>(args);
	} else if(coupling.stl_view() == "fplbm"){
		return lbm_main<FloatT,DescT,method::FpLbm>(args);
	} else if(coupling.stl_view() == "psm"){
		return lbm_main<FloatT,DescT,method::Psm>(args);
	}

	return saw::make_error<saw::err::critical>("Invalid coupling");
}


int main(int argc, char** argv){
	auto eov = k_main(argc, argv);
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
