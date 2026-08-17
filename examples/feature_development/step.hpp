#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

template<typename T, typename Desc, typename Coll>
saw::error_or<void> step(
		const converter<T>& conv,
		saw::data<sch::Ptr<sch::ChunkStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& fields,
		saw::data<sch::Ptr<sch::MacroStruct<T,Desc>>,encode::Sycl<saw::encode::Native>>& macros,
		saw::data<sch::Ptr<sch::ParticleSpheroidGroup<T,Desc>>,encode::Sycl<saw::encode::Native>>& particles,
		saw::data<sch::UInt64> t_i,
		device& dev
){
	auto& q = dev.get_handle();
	auto& info_f = fields.template get<"info">();
	auto& porous_f = macros.template get<"porosity">();
	
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

			hlbm_one_part.apply(fields,macros,particles,index,t_i,{16u});
		});
	}).wait();

	// auto coll_ev = 
	q.submit([&](acpp::sycl::handler& h){
		component<T,Desc,cmpt::Hlbm,encode::Sycl<saw::encode::Native>> collision{0.8};
		component<T,Desc,cmpt::BounceBack,encode::Sycl<saw::encode::Native>> bb;

		component<T,Desc,cmpt::ZouHeHorizontal<true>,encode::Sycl<saw::encode::Native>> flow_in{
			[&](){
				uint64_t target_t_i = 16u;
				if(t_i.get() < target_t_i){
					return 1.0 + (0.01 / target_t_i) * t_i.get();
				}
				return 1.01;
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

	return saw::make_void();
}

}
}
