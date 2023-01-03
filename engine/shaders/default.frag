#version 460
#include "lib/common.glsl"
#include "lib/gbuffer.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec4 g_position;
layout (location = 1) in vec4 g_normal;
layout (location = 2) in flat uvec2 g_texture_indices;

DECLARE_UBO(0, 0);
layout (set = 1, binding = 0) uniform sampler2D g_textures[];

layout (location = 0) out vec4 g_out_albedo;
layout (location = 1) out vec2 g_out_normal;

vec3 compute_normal(sampler2D map, vec3 position, vec3 normal, vec2 uv) {
    vec3 local_normal = vec3(texture(map, uv).rg, 0.0f) * 2.0f - 1.0f;
    local_normal.z = sqrt(1.0f - dot(local_normal.xy, local_normal.xy));

    vec3 dpos1 = dFdx(position);
    vec3 dpos2 = dFdy(position);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 N = normalize(normal);
    vec3 T = normalize(dpos1 * duv2.y - dpos2 * duv1.y);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * local_normal);
}

void main() {
    vec2 uv = vec2(g_position.w, g_normal.w);
    vec3 albedo = texture(g_textures[g_texture_indices.x], uv).rgb;
    vec3 normal = compute_normal(g_textures[g_texture_indices.y], g_position.xyz, g_normal.xyz, uv);
    g_out_albedo = vec4(albedo, 1.0f);
    g_out_normal = encode_normal(normal);
}
