#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>

#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_x = 1024u;
constexpr uint64_t dim_y = 512u;

namespace sch {
using namespace saw::schema;

using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y>;

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
	Member<RhoChunk<T>, "density">
>;
}

template<typename T, typename Desc>
saw::error_or<void> setup_initial_conditions(
		saw::data<sch::ChunkStruct<T,Desc>>& fields,
		saw::data<sch::MacroStruct<T,Desc>>& macros
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
	
	// Inflow
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(3u);
		},
		{{0u,0u}},
		{{1u, dim_y}},
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

	return saw::make_void();
}

template<typename T, typename Desc>
saw::error_or<void> step(
		saw::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& fields,
		saw::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& macros,
		saw::data<sch::UInt64> t_i,
		device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();

	// auto coll_ev = 
	q.submit([&](acpp::sycl::handler& h){
		// Need nicer things to handle the flow. I see improvement here
		component<T,Desc,cmpt::BGK,encode::Sycl<saw::encode::Native>> collision{0.6};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;
		component<T,Desc,cmpt::ZouHeHorizontal<true>,encode::Sycl<saw::encode::Native>> flow_in{1.001};
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

	converter<sch::Float64> conv {
		// delta_x
		{{1.0}},
		// delta_t
		{{1.0}}
	};

	// saw::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	auto lbm_data_ptr = saw::heap<saw::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = saw::heap<saw::data<sch::MacroStruct<T,Desc>>>();

	device dev;

	auto& sycl_q = dev.get_handle();

	sycl_q.wait();
	{
		auto eov = setup_initial_conditions<T,Desc>(*lbm_data_ptr,*lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	saw::data<sch::ChunkStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_data{sycl_q};
	saw::data<sch::MacroStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_macro_data{sycl_q};
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
	sycl_q.wait();
	saw::data<sch::UInt64> time_steps{256ul};

	for(saw::data<sch::UInt64> i{0u}; i < time_steps; ++i){
		{
			std::string file_name = "tmp/t_";
			file_name += std::to_string(i.get());
			file_name += ".vtk";
			auto eov = write_vtk_file(file_name, *lbm_macro_data_ptr);
			if(eov.is_error()){
				return eov;
			}
		}
		{
			std::string file_name = "tmp/df_";
			file_name += std::to_string(i.get());
			file_name += ".vtk";
			auto eov = write_vtk_file(file_name, *lbm_data_ptr);
			if(eov.is_error()){
				return eov;
			}
		}
		{
			auto eov = step<T,Desc>(lsd_view,lsdm_view,i,dev);
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
	}
	sycl_q.wait();
	{
		std::string file_name = "tmp/t_";
		file_name += std::to_string(time_steps.get());
		file_name += ".vtk";
		auto eov = write_vtk_file(file_name, *lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		std::string file_name = "tmp/df_";
		file_name += std::to_string(time_steps.get());
		file_name += ".vtk";
		auto eov = write_vtk_file(file_name, *lbm_data_ptr);
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
