#pragma once

#include <string_view>

namespace kel {
namespace lbm {
constexpr std::string_view shader_basic_vertex = R"(
#version 330 core

layout(location=0) in vec2 vertices;
layout(location=1) in vec2 v_uv;

out vec2 uv;

void main(){
	uv = v_uv;
	gl_Position = vec4(vertices, 0.0, 1.0);
}
)";

constexpr std::string_view shader_basic_fragment = R"(
#version 330 core

in vec2 uv;
out vec4 color;

uniform sampler2D tex_sampler;

void main(){
	color = texture(tex_sampler, uv);
	color.r = 0.8;
}
)";

}
}
