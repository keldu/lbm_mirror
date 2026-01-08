#include <kel/lbm/lbm.hpp>
#include <AdaptiveCpp/sycl/sycl.hpp>


#include <kel/lbm/particle/geometry/circle.hpp>
#include <iostream>

namespace kel{
namespace lbm {
namespace sch {
using namespace saw::schema;
}
}

saw::error_or<void> lbm_main(int argc, char** argv){
	(void) argc;
	(void) argv;

	using namespace lbm;
	using namespace acpp;

	saw::data<sch::Array<sch::Particle<sch::Float32,2u>>> particles{1024u};

	for(saw::data<sch::UInt64> i{0u}; i < saw::data<sch::UInt64>{32u}; ++i){
		for(saw::data<sch::UInt64> j{0u}; j < saw::data<sch::UInt64>{32u}; ++j){

			auto& part = particles.at(i*32ul+j);
			auto& body = part.template get<"rigid_body">();

			auto& pos = body.template get<"position">();
			auto& old_pos = body.template get<"position_old">();
			auto& acceleration = body.template get<"acceleration">();
			auto& p_size = part.template get<"size">();
			auto& p_rad = part.template get<"collision">().template get<"radius">();
			p_size = {0.4f};
			p_rad = {0.4f};

			if(j.get() % 2u == 0) acceleration.at({{1u}}) = {-9.81};

			pos.at({{0u}}) = {i.template cast_to<sch::Float32>()+0.5f};
			pos.at({{1u}}) = {j.template cast_to<sch::Float32>()+64.0f};

			old_pos = pos;
		}
	}

	for(saw::data<sch::UInt64> dt{0u}; dt < saw::data<sch::UInt64>{32ul}; ++dt){
		// Do Verlet Step
		for(saw::data<sch::UInt64> i{0u}; i < particles.size(); ++i){
			auto& part_i = particles.at(i);

			verlet_step_lambda<sch::Float32,2u>(part_i,{0.05f});
			auto& body_i = part_i.template get<"rigid_body">();
			auto& acc_i = body_i.template get<"acceleration">();

			acc_i.at({{0u}}) = 0.0;
			acc_i.at({{1u}}) = -9.81;
		}
		//
		for(saw::data<sch::UInt64> i{0u}; i < particles.size(); ++i){
			auto& part_i = particles.at(i);
			auto& body_i = part_i.template get<"rigid_body">();
			auto& pos_i = body_i.template get<"position">();
			auto& pos_old_i = body_i.template get<"position_old">();
			/**
			 * Test against other particles
			 */
			for(saw::data<sch::UInt64> j{i+1ul}; j < particles.size(); ++j){
				auto& part_j = particles.at(j);
				auto& body_j = part_j.template get<"rigid_body">();
				auto& pos_j = body_j.template get<"position">();
				auto& pos_old_j = body_j.template get<"position_old">();

				auto res = broadphase_collision_distance<sch::Float32,2u>(part_i, part_j);
				if(res.first){
					//std::cout<<"Collision"<<std::endl;
					// Do collision

					auto vel_i = pos_i - pos_old_i;
					auto vel_j = pos_j - pos_old_j;

					auto vel_rel = vel_i - vel_j;
					auto pos_rel = pos_i - pos_j;

					auto vel_pos_rel_dot = saw::math::dot(vel_rel,pos_rel);
					if(vel_pos_rel_dot.get() < 0.0){

					}

					auto vel_ij = saw::math::dot(vel_i,vel_j);

				}
			}
			/**
			 * Test against walls
			 */
			if(pos_i.at({{0u}}).get() <= 0 or pos_i.at({{0u}}).get() >= 40 ){
				auto pos_i_0 = pos_i.at({{0u}});
				pos_i.at({{0u}}) = pos_old_i.at({{0u}});
				pos_old_i.at({{0u}}) = pos_i_0;
			}
			if(pos_i.at({{1u}}).get() <= 0 ){
				auto pos_i_1 = pos_i.at({{1u}});
				pos_i.at({{1u}}) = pos_old_i.at({{1u}});
				pos_old_i.at({{1u}}) = pos_i_1;
			}
		}
		auto& pos = particles.at({0u}).template get<"rigid_body">().template get<"position">();

		std::cout<<pos.at({{0u}}).get()<<" "<<pos.at({{1u}}).get()<<std::endl;
	}

	return saw::make_void();
}
}

int main(int argc, char** argv){
	auto eov = kel::lbm_main(argc, argv);
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
