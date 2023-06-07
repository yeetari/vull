#version 460
#include "lib/common.glsl"

layout (location = 0) in vec3 g_position;
layout (set = 0, binding = 1) uniform samplerCube g_cube_map;
layout (location = 0) out vec4 g_out_colour;

void main() {
    g_out_colour = vec4(texture(g_cube_map, g_position).rgb, 1.0f);
}
