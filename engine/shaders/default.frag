#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec4 g_position;
layout (location = 1) in vec4 g_normal;
layout (location = 2) in flat uvec2 g_texture_indices;

DECLARE_UBO(0);
layout (set = 1, binding = 0) uniform sampler2D g_textures[];

layout (location = 0) out vec4 g_out_albedo;
layout (location = 1) out vec4 g_out_normal;

void main() {
    vec2 uv = vec2(g_position.w, g_normal.w);
    vec3 albedo = texture(g_textures[g_texture_indices.x], uv).rgb;
    vec3 normal = compute_normal(g_textures[g_texture_indices.y], g_position.xyz, g_normal.xyz, uv);
    g_out_albedo = vec4(albedo, 1.0f);
    g_out_normal = vec4(normal, 1.0f);
}
