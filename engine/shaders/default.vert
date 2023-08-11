#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

DECLARE_UBO(0, 0);
DECLARE_DRAW_BUFFER(0, 3);
DECLARE_OBJECT_BUFFER(0, 1);
DECLARE_VERTEX_BUFFER(0, 5);

layout (location = 0) out vec4 g_out_position;
layout (location = 1) out vec4 g_out_normal;
layout (location = 2) out flat uvec2 g_out_texture_indices;

void main() {
    Vertex vertex = g_vertices[gl_VertexIndex];
    Object object = g_objects[g_draws[gl_DrawID].object_index];
    vec4 world_position = object.transform * vec4(vertex.position, 1.0f);
    g_out_position = vec4(world_position.xyz, vertex.uv.x);
    g_out_normal = vec4(adjugate(object.transform) * vertex.normal, vertex.uv.y);
    g_out_texture_indices = uvec2(object.albedo_index, object.normal_index);
    gl_Position = g_proj_view * world_position;
}
