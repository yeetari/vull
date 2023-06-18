#version 460
#include "lib/common.glsl"

layout (location = 0) in vec2 g_uv;
layout (location = 1) in vec4 g_colour;

layout (binding = 0) uniform sampler2D g_textures[];

layout (location = 0) out vec4 g_out_colour;

layout (push_constant) uniform PushConstants {
    uvec2 g_viewport;
    uint g_texture_index;
};

void main() {
    vec4 texel = texture(g_textures[g_texture_index], g_uv);
    g_out_colour = g_colour * texel;
}
