#version 460
#include "lib/common.glsl"
#include "lib/gbuffer.glsl"
#include "lib/ubo.glsl"
#include "terrain.glsl"

layout (location = 0) in vec3 g_world_position;

layout (location = 0) out vec4 g_out_albedo;
layout (location = 1) out vec2 g_out_normal;

void main() {
    float colour_height = max((g_world_position.y / 10.0f) - 0.25f, 0.0f);
    vec3 albedo = mix(vec3(0.15f, 0.4f, 0.08f), vec3(0.23f, 0.61f, 0.12f), colour_height);
    g_out_albedo = vec4(0.0f, 1.0f, 0.0f, 1.0f);

    vec2 e = vec2(0.01, 0.0);
    vec3 normal = normalize(vec3(sample_height(g_world_position.xz - e) - sample_height(g_world_position.xz + e), 2.0 * e.x, sample_height(g_world_position.xz - e.yx) - sample_height(g_world_position.xz + e.yx)));
    g_out_normal = encode_normal(normal);
}
