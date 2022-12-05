#version 460
#include "lib/common.glsl"
#include "ui.glsl"

layout (location = 0) out FragmentData {
    vec2 uv;
    flat uint object_id;
} g_fragment;

const vec2 k_vertices[6] = vec2[6](
    vec2(-1.0f, -1.0f),
    vec2(1.0f, -1.0f),
    vec2(1.0f, 1.0f),
    vec2(1.0f, 1.0f),
    vec2(-1.0f, 1.0f),
    vec2(-1.0f, -1.0f)
);

void main() {
    UiObject object = g_data.objects[nonuniformEXT(gl_InstanceIndex)];
    g_fragment.object_id = gl_InstanceIndex;

    vec2 position = (k_vertices[gl_VertexIndex] + 1.0f) * 0.5f;
    g_fragment.uv = position;

    position *= object.scale;
    position += object.position;
    position *= round(g_data.global_scale * 10.0f) / 10.0f;
    position /= vec2(k_viewport_width, k_viewport_height);
    gl_Position = vec4(position * 2.0f - 1.0f, 0.0f, 1.0f);
}
