#include <kel/lbm/lbm.hpp>

#include "state.hpp"

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
}

}

saw::error_or<void> lbm_main(int argc, char** argv){
	auto eo_state = lbm::create_ogl_renderer();
	if(eo_state.is_error()){
		return std::move(eo_state.get_error());
	}
	auto& state = *(eo_state.get_value());

	bool running{true};
	while(running){
		state.clear();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		SDL_GL_SwapWindow(state.window);

		SDL_Event event;
		while(SDL_PollEvent(&event)){
			switch(event.type){
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_KEY_UP:
				if(event.key.key== SDLK_ESCAPE){
					running = false;
				}
				break;
			default:
				break;
			}
		}
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
