#include <kel/lbm/lbm.hpp>
#include <kel/lbm/sycl/lbm.hpp>

#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_x = 256u;
constexpr uint64_t dim_y = 16u;

namespace sch {
using namespace saw::schema;

template<typename T, typename Desc>
using CellStruct = Struct<
	Member<FixedArray<T,Desc::Q>, "dfs">,
	Member<FixedArray<T,Desc::Q>, "dfs_old">,
	Member<UInt8, "info">,
	Member<Vector<T,Desc::D>, "velocity">,
	Member<Vector<T,Desc::D>, "force">
>;


using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y>;

template<typename T, typename Desc>
using ChunkStruct = Struct<
	Member<InfoChunk, "info">
	//,Member<DfChunk<T,Desc>, "dfs">
>;
}

template<typename T, typename Desc>
saw::error_or<void> setup_initial_conditions(saw::data<sch::ChunkStruct<T,Desc>>& fields){
	auto& info_f = fields.template get<"info">();
	// Set everything as walls
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(2u);
		},
		{},
		info_f.get_dims(),
		{}
	);
	// Fluid
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(1u);
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
/*
	//
	auto& df_f = fields.template get<"dfs">();
	saw::data<T> rho{1};
	saw::data<sch::FixedArray<T,Desc::D>> vel{};
	auto eq = equilibrium<T,Desc>(rho,vel);

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims()
	);
*/

	return saw::make_void();
}

template<typename T, typename Desc>
saw::error_or<void> step(saw::data<sch::ChunkStruct<T,Desc>,encode::Sycl<saw::encode::Native>>& fields, saw::data<sch::UInt64> t_i, device& dev){
	auto& q = dev.get_handle();

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
	saw::data<sch::ChunkStruct<T,Desc>> lbm_data{};

	device dev;
	auto& sycl_q = dev.get_handle();

	sycl_q.wait();
	{
		auto eov = setup_initial_conditions<T,Desc>(lbm_data);
		if(eov.is_error()){
			return eov;
		}
	}

	saw::data<sch::ChunkStruct<T,Desc>, encode::Sycl<saw::encode::Native>> lbm_sycl_data;
	{
		auto eov = dev.allocate_on_device(lbm_sycl_data);
		if(eov.is_error()){
			return eov;
		}
	}
	{
		auto eov = dev.copy_to_device(lbm_data,lbm_sycl_data);
		if(eov.is_error()){
			return eov;
		}
	}

	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{32ul}; ++i){
		auto eov = step<T,Desc>(lbm_sycl_data,i,dev);
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
