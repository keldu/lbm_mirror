#include <kel/lbm/sycl/lbm.hpp>
#include <kel/lbm/lbm.hpp>

#include <forstio/io/io.hpp>
#include <forstio/remote/filesystem/easy.hpp>
#include <forstio/codec/json/json.hpp>
#include <forstio/codec/simple.hpp>
#include <forstio/codec/args.hpp>

namespace kel {
namespace lbm {

// Square cavity, as this case has always used.
constexpr uint64_t dim_x = 128ul;
constexpr uint64_t dim_y = 128ul;

// Lid velocity, tangential along +x on the top row.
constexpr double lid_velocity = 0.1;

namespace sch {

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
	Member<RhoChunk<T>, "density">
>;

//template<typename T, typename Desc>
//using ParticleArray = Array<
//	Particle<T,Desc::D>
//>;
}

template<typename T, typename Desc>
kel::lbm::error_or<void> setup_initial_conditions(
		kel::lbm::data<sch::ChunkStruct<T,Desc>>& fields,
		kel::lbm::data<sch::MacroStruct<T,Desc>>& macros
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
	
	// The lid: the top row, moving tangentially. Corners stay plain walls.
	iterator<Desc::D>::apply(
		[&](auto& index){
			info_f.at(index).set(5u);
		},
		{{0u,dim_y-1u}},
		{{dim_x,dim_y}},
		{{1u,0u}}
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

	iterator<Desc::D>::apply(
		[&](auto& index){
			auto& df = df_f.at(index);
			auto& rho = rho_f.at(index);
			rho.at({}) = {1};
			auto& vel = vel_f.at(index);
			if(info_f.at(index).get() == 2u){
				vel.at({{0u}}) = 0.0;
			}
			auto eq = equilibrium<T,Desc>(rho,vel);

			df = eq;
		},
		{},// 0-index
		df_f.get_dims(),
		{{1u,1u}}
	);
	
	iterator<Desc::D>::apply(
		[&](auto& index){
			kel::lbm::data<sch::Vector<T,Desc::D>> middle, ind_vec;
			middle.at({{0u}}) = dim_x * 0.25;
			middle.at({{1u}}) = dim_y * 0.5;

			ind_vec.at({{0u}}) = index.at({{0u}}).template cast_to<T>();
			ind_vec.at({{1u}}) = index.at({{1u}}).template cast_to<T>();

			auto dist = middle - ind_vec;
			auto dist_2 = kel::lbm::math::dot(dist,dist);
			if(dist_2.at({}).get() < dim_y*dim_y*0.01){
				info_f.at(index) = 1u;
			}
		},
		{},// 0-index
		df_f.get_dims()
	);

	return kel::lbm::make_void();
}

template<typename T, typename Desc>
kel::lbm::error_or<void> step(
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
		component<T,Desc,cmpt::BGK, encode::Sycl<kel::lbm::enc::Native>> collision{0.8};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<kel::lbm::enc::Native>> bb;
		component<T,Desc,cmpt::AntiBounceBack<0u>,encode::Sycl<kel::lbm::enc::Native>> abb;

		kel::lbm::data<sch::Scalar<T>> rho_b;
		rho_b.at({}) = 1.0;
		kel::lbm::data<sch::Vector<T,Desc::D>> vel_b;
		vel_b.at({{0u}}) = 0.015;

		component<T,Desc,cmpt::Equilibrium,encode::Sycl<kel::lbm::enc::Native>> equi{rho_b,vel_b};

		component<T,Desc,cmpt::ZouHeHorizontal<true>,encode::Sycl<kel::lbm::enc::Native>> flow_in{
			[&](){
				uint64_t target_t_i = 32u;
				if(t_i.get() < target_t_i){
					return 1.0 + (0.0002 / target_t_i) * t_i.get();
				}
				return 1.0002;
			}()
		};
		component<T,Desc,cmpt::ZouHeHorizontal<false>,encode::Sycl<kel::lbm::enc::Native>> flow_out{1.0};


		h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
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
				case 5u: {
					/**
					 * Moving wall: bounce back plus the tangential momentum the
					 * lid injects.
					 *
					 *   f_i = f_opp + 2 w_i rho_w (e_i . u_w) / cs^2
					 *
					 * written from f_opp because that is the direction the
					 * library's BounceBack writes in. There is no component for a
					 * y normal moving wall, and the original example had this
					 * commented out, so it is implemented here.
					 */
					using dfi = df_info<T,Desc>;
					const bool is_even = ((t_i.get() % 2u) == 0u);
					auto& dfs_f = (is_even)
						? fields.template get<"dfs_old">()
						: fields.template get<"dfs">();

					auto& dfs = dfs_f.at(index);
					auto df_cpy = dfs;

					for(uint64_t i = 0u; i < Desc::Q; ++i){
						const auto e_dot_u =
							static_cast<typename kel::lbm::native_data_type<T>::type>(
								dfi::directions[i][0]) * lid_velocity;

						dfs.at({i}) = df_cpy.at({dfi::opposite_index.at(i)})
							+ kel::lbm::data<T>{
								static_cast<typename kel::lbm::native_data_type<T>::type>(
									2.0 * dfi::weights[i] * dfi::inv_cs2 * e_dot_u)};
					}
				} break;
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

	return kel::lbm::make_void();
}
}
}

template<typename T, typename Desc>
kel::lbm::error_or<void> lbm_main(int argc, char** argv){
	using namespace kel::lbm;

	/*
	auto eo_lbm_args = setup_lbm_env<sch::Args<sch::Struct<>,sch::Tuple<>>>(argc,argv);
	if(eo_lbm_args.is_error()){
		return std::move(eo_lbm_args.get_error());
	}
	auto& lbm_args = eo_lbm_args.get_value();
	*/

	using dfi = df_info<T,Desc>;

	auto eo_lbm_dir = output_directory();
	if(eo_lbm_dir.is_error()){
		return std::move(eo_lbm_dir.get_error());
	}
	auto& lbm_dir = eo_lbm_dir.get_value();

	auto out_dir = lbm_dir / "cavity_2d_gpu";

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
	
	print_lbm_meta<T,Desc>(conv,{0.1},{1e-4},{0.4 * dim_y});

	// kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> meta{{dim_x,dim_y}};
	auto lbm_data_ptr = kel::lbm::heap<kel::lbm::data<sch::ChunkStruct<T,Desc>>>();
	auto lbm_macro_data_ptr = kel::lbm::heap<kel::lbm::data<sch::MacroStruct<T,Desc>>>();
	
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
		auto eov = setup_initial_conditions<T,Desc>(*lbm_data_ptr,*lbm_macro_data_ptr);
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

	kel::lbm::data<sch::ChunkStruct<T,Desc>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_data{sycl_q};
	kel::lbm::data<sch::MacroStruct<T,Desc>, encode::Sycl<kel::lbm::enc::Native>> lbm_sycl_macro_data{sycl_q};
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
	sycl_q.wait();
	auto lsd_view = make_view(lbm_sycl_data);
	auto lsdm_view = make_view(lbm_sycl_macro_data);
	kel::lbm::data<sch::UInt64> time_steps{16u*4096ul};

	auto& info_f = lsd_view.template get<"info">();

	for(kel::lbm::data<sch::UInt64> i{0u}; i < time_steps and krun; ++i){
		// BC + Collision
		{
			auto eov = step<T,Desc>(lsd_view,lsdm_view,i,dev);
			if(eov.is_error()){
				return eov;
			}
		}
		sycl_q.wait();
		/*
		if(i.get() % 128u == 0u){
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
		*/
		/*
		if(i.get() % 32u == 0u){
			{
				auto eov = dev.copy_to_host(lbm_sycl_macro_data,*lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
			{
				auto eov = write_csv_file(out_dir,"m",i.get(), *lbm_macro_data_ptr);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		*/
		// Stream
		sycl_q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::Stream,encode::Sycl<kel::lbm::enc::Native>> stream;

			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				kel::lbm::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
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
	{
		auto eov = write_csv_file(out_dir,"m",time_steps.get(), *lbm_macro_data_ptr);
		if(eov.is_error()){
			return eov;
		}
	}

	sycl_q.wait();
	return kel::lbm::make_void();
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
