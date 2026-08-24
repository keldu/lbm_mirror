#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, typename Desc, typename Coll>
saw::error_or<void> step(
		const converter<T>& conv,
		const saw::data<sch::Scalar<T>>& tau,
		saw::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& fields,
		saw::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& macros,
		saw::data<sch::Ptr<sch::ParticleSpheroidGroup<T,Desc>>,encode::Sycl<saw::encode::Native>>& particles,
		saw::data<sch::UInt64> t_i,
		device& dev
){
	static constexpr saw::data<sch::UInt64> sub_steps{0u};

	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();
	auto& porous_f = macros.template get<"porosity">();


	
	if constexpr ( std::is_same_v<Coll, method::Hlbm> ){
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::HlbmReset,encode::Sycl<saw::encode::Native>> hlbm_reset;
			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
				for(uint64_t i = 0u; i < Desc::D; ++i){
					index.at({{i}}).set(idx[i]);
				}

				hlbm_reset.apply(fields,macros,index,t_i);
			});
		}).wait();
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::HlbmOneParticle,encode::Sycl<saw::encode::Native>> hlbm_one_part;

			h.parallel_for(acpp::sycl::range<1u>{particle_amount}, [=](acpp::sycl::id<1u> idx){
				saw::data<sch::FixedArray<sch::UInt64,1u>> index;
				for(uint64_t i = 0u; i < 1u; ++i){
					index.at({{i}}).set(idx[i]);
				}

				hlbm_one_part.apply(fields,macros,particles,index,t_i,{0u});
			});
		}).wait();

		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::Hlbm,encode::Sycl<saw::encode::Native>> collision{0.75};
			component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;
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
						{
							component<T,Desc,cmpt::ZouHeVelocityX<true>,encode::Sycl<saw::encode::Native>> flow_in{
								[&]() -> saw::data<sch::Vector<T,Desc::D>> {
									saw::data<sch::Vector<T,Desc::D>> vel;
									saw::data<T> scale;
									{
										scale.set([&](){
											if(t_i.get() > 2048u){
												return 1.0f;
											}

											return t_i.get() / 2048.0f;
										}());
									}
									{
										auto y_si = conv.meter_lbm_to_si(index.at({{1u}}).template cast_to<T>()).handle();
										vel.at({{0u}}) = conv.velocity_si_to_lbm({{static_cast<typename saw::native_data_type<T>::type>((1.2 / 0.41) * y_si.get() - (1.2 / (0.41*0.41)) * y_si.get() * y_si.get())}}).handle() * scale;
										vel.at({{1u}}) = 0.0f;
									}
									return vel;
								}()
							};
							flow_in.apply(fields,index,t_i);
						}
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
	}else if constexpr ( std::is_same_v<Coll, method::FpLbm> ){
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::FpLbmReset,encode::Sycl<saw::encode::Native>> fplbm_reset;
			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
				for(uint64_t i = 0u; i < Desc::D; ++i){
					index.at({{i}}).set(idx[i]);
				}
				
				fplbm_reset.apply(fields,macros,index,t_i);
			});
		}).wait();

		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::FpLbmOneParticle,encode::Sycl<saw::encode::Native>> fplbm_one_part;

			h.parallel_for(acpp::sycl::range<1u>{particle_amount}, [=](acpp::sycl::id<1u> idx){
				saw::data<sch::FixedArray<sch::UInt64,1u>> index;
				for(uint64_t i = 0u; i < 1u; ++i){
					index.at({{i}}).set(idx[i]);
				}

				fplbm_one_part.apply(fields,macros,particles,index,t_i,{0u});
			});
		}).wait();

		// auto coll_ev = 
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::BGK,encode::Sycl<saw::encode::Native>> bgk{0.75};
			component<T,Desc,cmpt::FpLbm,encode::Sycl<saw::encode::Native>> collision{0.75};
			component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;
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
						{
							component<T,Desc,cmpt::ZouHeVelocityX<true>,encode::Sycl<saw::encode::Native>> flow_in{
								[&]() -> saw::data<sch::Vector<T,Desc::D>> {
									saw::data<sch::Vector<T,Desc::D>> vel;
									{
										auto y_si = conv.meter_lbm_to_si(index.at({{1u}}).template cast_to<T>()).handle();
										vel.at({{0u}}) = conv.velocity_si_to_lbm({{static_cast<typename saw::native_data_type<T>::type>((1.2 / 0.41) * y_si.get() - (1.2 / (0.41*0.41)) * y_si.get() * y_si.get())}}).handle();
										vel.at({{1u}}) = 0.0f;
									}
									return vel;
								}()
							};
							flow_in.apply(fields,index,t_i);
						}
						bgk.apply(fields,macros,index,t_i);
						break;
					case 4u:
						flow_out.apply(fields,index,t_i);
						bgk.apply(fields,macros,index,t_i);
					break;
					default:
					break;
				}
			});
		}).wait();
	}else if constexpr ( std::is_same_v<Coll, method::Psm> ){
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::PsmReset,encode::Sycl<saw::encode::Native>> fplbm_reset;
			h.parallel_for(acpp::sycl::range<Desc::D>{dim_x,dim_y}, [=](acpp::sycl::id<Desc::D> idx){
				saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
				for(uint64_t i = 0u; i < Desc::D; ++i){
					index.at({{i}}).set(idx[i]);
				}
				
				fplbm_reset.apply(fields,macros,index,t_i);
			});
		}).wait();

		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::PsmOneParticle,encode::Sycl<saw::encode::Native>> fplbm_one_part;

			h.parallel_for(acpp::sycl::range<1u>{particle_amount}, [=](acpp::sycl::id<1u> idx){
				saw::data<sch::FixedArray<sch::UInt64,1u>> index;
				for(uint64_t i = 0u; i < 1u; ++i){
					index.at({{i}}).set(idx[i]);
				}

				fplbm_one_part.apply(fields,macros,particles,index,t_i,{0u});
			});
		}).wait();

		// auto coll_ev = 
		q.submit([&](acpp::sycl::handler& h){
			component<T,Desc,cmpt::BGK,encode::Sycl<saw::encode::Native>> bgk{0.75};
			component<T,Desc,cmpt::Psm,encode::Sycl<saw::encode::Native>> collision{0.75};
			component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;
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
						{
							component<T,Desc,cmpt::ZouHeVelocityX<true>,encode::Sycl<saw::encode::Native>> flow_in{
								[&]() -> saw::data<sch::Vector<T,Desc::D>> {
									saw::data<sch::Vector<T,Desc::D>> vel;
									{
										auto y_si = conv.meter_lbm_to_si(index.at({{1u}}).template cast_to<T>()).handle();
										vel.at({{0u}}) = conv.velocity_si_to_lbm({{static_cast<typename saw::native_data_type<T>::type>((1.2 / 0.41) * y_si.get() - (1.2 / (0.41*0.41)) * y_si.get() * y_si.get())}}).handle();
										vel.at({{1u}}) = 0.0f;
									}
									return vel;
								}()
							};
							flow_in.apply(fields,index,t_i);
						}
						bgk.apply(fields,macros,index,t_i);
						break;
					case 4u:
						flow_out.apply(fields,index,t_i);
						bgk.apply(fields,macros,index,t_i);
					break;
					default:
					break;
				}
			});
		}).wait();

	}

	return saw::make_void();
}

}
}
