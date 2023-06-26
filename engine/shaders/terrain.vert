#version 460
#include "lib/common.glsl"
#include "lib/ubo.glsl"
#include "terrain.glsl"

layout (location = 0) in vec2 g_position;
layout (location = 0) out vec3 g_world_position;

void main() {
    g_world_position.xz = g_position * vec2(g_chunk_size) + g_translation;
    g_world_position.xz = min(g_world_position.xz, vec2(g_terrain_size));
    g_world_position.y = sample_height(g_world_position.xz);

    float camera_distance = distance(g_world_position, g_view_position);
    float morph = 1.0f - clamp(g_morph_const_z - camera_distance * g_morph_const_w, 0.0f, 1.0f);

    vec3 grid_dim = vec3(2.0f, 1.0f, 1.0f);
    vec2 fraction = fract(g_position * grid_dim.y) * grid_dim.z * vec2(g_chunk_size);
    g_world_position.xz -= fraction * morph;
    g_world_position.y = sample_height(g_world_position.xz);
    gl_Position = g_proj_view * vec4(g_world_position, 1.0f);
}
