#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

DECLARE_UBO(0);

layout (location = 0) out FragmentData g_fragment;

void main() {
    vec4 world_position = object_transform() * vec4(g_position, 1.0f);
    g_fragment.position = world_position.xyz;
    g_fragment.normal = adjugate(object_transform()) * g_normal;
    g_fragment.uv = g_uv;
    gl_Position = g_proj * g_view * world_position;
}
