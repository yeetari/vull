#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

DECLARE_UBO(0, 0);
DECLARE_DRAW_BUFFER(0, 3);
DECLARE_OBJECT_BUFFER(0, 4);

layout (location = 0) out vec4 g_out_position;
layout (location = 1) out vec4 g_out_normal;
layout (location = 2) out flat uvec2 g_out_texture_indices;

void main() {
    Object object = g_objects[g_draws[gl_DrawID].object_index];
    vec4 world_position = object.transform * vec4(g_position, 1.0f);
    g_out_position = vec4(world_position.xyz, g_uv.x);
    g_out_normal = vec4(adjugate(object.transform) * g_normal, g_uv.y);
    g_out_texture_indices = uvec2(object.albedo_index, object.normal_index);
    gl_Position = g_proj_view * world_position;
}
