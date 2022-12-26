#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

DECLARE_UBO(0);
layout (binding = 3, scalar) readonly buffer DrawBuffer {
    DrawCmd g_draws[];
};

layout (location = 0) out vec4 g_out_position;
layout (location = 1) out vec4 g_out_normal;
layout (location = 2) out flat uvec2 g_out_texture_indices;

void main() {
    DrawCmd draw = g_draws[gl_DrawID];
    vec4 world_position = draw.transform * vec4(g_position, 1.0f);
    g_out_position = vec4(world_position.xyz, g_uv.x);
    g_out_normal = vec4(adjugate(draw.transform) * g_normal, g_uv.y);
    g_out_texture_indices = uvec2(draw.albedo_index, draw.normal_index);
    gl_Position = g_proj * g_view * world_position;
}
