#pragma once

#include "common.hpp"
#include "program.hpp"

#include "shaders/basic.hpp"

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
}

struct ogl_state final {
public:
	SDL_Window* window;
	SDL_GLContext context;
	int version;
	GLuint vao_id;
	GLuint vbo_id;
	GLuint veo_id;

	GLuint program_id;
	GLuint fbo_id;
	GLuint tex_id;

	uint64_t w, h;

public:
	ogl_state():
		window{nullptr},
		version{0},
		vao_id{0u},
		vbo_id{0u},
		program_id{0u},
		fbo_id{0u},
		tex_id{0u},
		w{1024},
		h{1024}
	{
		SDL_Init(SDL_INIT_VIDEO);
	}

	~ogl_state(){
		SDL_GL_DestroyContext(context);
		if(window){
			SDL_DestroyWindow(window);
			window = nullptr;
		}
		SDL_Quit();
	}

	void clear(){
		glClear(GL_COLOR_BUFFER_BIT);
	}
};

saw::error_or<saw::own<ogl_state>> create_ogl_renderer(){
	auto owned_state = saw::heap<ogl_state>();

	auto& st = *owned_state;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	st.window = SDL_CreateWindow("Kel'LBM Renderer",st.w,st.h,SDL_WINDOW_OPENGL /*| SDL_WINDOW_HIDDEN*/);
	if(not st.window){
		return saw::make_error<saw::err::critical>("Window not created");
	}
	st.context = SDL_GL_CreateContext(st.window);

	st.version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
	glGenVertexArrays(1,&st.vao_id);
	glBindVertexArray(st.vao_id);
	glClearColor(0.0,0.0,0.0,1.0);

	glGenBuffers(1,&st.vbo_id);
	glGenBuffers(1,&st.veo_id);
	static constexpr GLfloat vertex_buffer_data[] = {
		-1.f, -1.f, 0.f, 0.f,
		 1.f, -1.f, 1.f, 0.f,
		 1.f,  1.f, 1.f, 1.f,
		-1.f,  1.f, 0.f, 1.f
	};
	static unsigned int index_buffer_data[] = {
		0, 1, 2,
		2, 3, 0
	};
	glBindBuffer(GL_ARRAY_BUFFER, st.vbo_id);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, st.veo_id);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glGenFramebuffers(1,&st.fbo_id);
	//glBindFramebuffer(GL_FRAMEBUFFER, st.fbo_id);

	glGenTextures(1,&st.tex_id);
	glBindTexture(GL_TEXTURE_2D,st.tex_id);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		1024,
		1024,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		NULL
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		st.tex_id,
		0
	);

	glViewport(0,0,1024,1024);
	st.program_id = create_ogl_program(shader_basic_vertex.data(), shader_basic_fragment.data());
	glUseProgram(st.program_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, st.tex_id);

	glUniform1i(glGetUniformLocation(st.program_id, "tex_sampler"), 0);
	st.clear();

	return owned_state;
}
}
}
