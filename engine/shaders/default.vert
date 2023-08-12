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

    // Unpack FP16 position.
    vec3 position = vec3(0);
    position.x = half_to_float(vertex.px);
    position.y = half_to_float(vertex.py);
    position.z = half_to_float(vertex.pz);

    // Unpack SNORM10 normal.
    vec3 normal = vec3((vertex.normal >> 20) & 0x3ff, (vertex.normal >> 10) & 0x3ff, vertex.normal & 0x3ff);
    normal -= vec3(512.0f);
    normal *= 0.0019569471624266144f;
    normal = max(normal, vec3(-1.0f));

    // Unpack FP16 texture coords.
    // TODO: Unpack in fragment shader?
    vec2 uv = unpackHalf2x16(vertex.uv);

    Object object = g_objects[g_draws[gl_DrawID].object_index];
    vec4 world_position = object.transform * vec4(position, 1.0f);
    g_out_position = vec4(world_position.xyz, uv.x);
    g_out_normal = vec4(adjugate(object.transform) * normal, uv.y);
    g_out_texture_indices = uvec2(object.albedo_index, object.normal_index);
    gl_Position = g_proj_view * world_position;
}
