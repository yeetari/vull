#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in FragmentData g_fragment;

DECLARE_UBO(0);
layout (set = 1, binding = 0) uniform sampler2D g_textures[];

layout (location = 0) out vec4 g_out_albedo;
layout (location = 1) out vec4 g_out_normal;

void main() {
    vec3 albedo = texture(g_textures[nonuniformEXT(object_albedo_index())], g_fragment.uv).rgb;
    vec3 normal = compute_normal(g_textures[nonuniformEXT(object_normal_index())], g_fragment);
    g_out_albedo = vec4(albedo, 1.0f);
    g_out_normal = vec4(normal, 1.0f);
}
