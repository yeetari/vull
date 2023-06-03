#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

layout (location = 0) out vec3 g_out_normal;

layout (push_constant) uniform PushConstantData {
    mat4 g_matrix;
};

void main() {
    g_out_normal = g_normal;
    gl_Position = g_matrix * vec4(g_position, 1.0f);
}
