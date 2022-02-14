#version 460
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

struct UiObject {
    vec2 position;
    uint glyph_index;
    float padding;
};

layout (binding = 0) readonly buffer UiData {
    UiObject g_objects[];
};
layout (binding = 1) uniform sampler2D g_font_samplers[];

layout (location = 0) out FragmentData {
    vec2 uv;
    flat uint glyph_index;
} g_fragment;

const vec2 k_vertices[6] = vec2[6](
    vec2(-1.0f, -1.0f),
    vec2(1.0f, -1.0f),
    vec2(1.0f, 1.0f),
    vec2(-1.0f, -1.0f),
    vec2(-1.0f, 1.0f),
    vec2(1.0f, 1.0f)
);

void main() {
    UiObject object = g_objects[nonuniformEXT(gl_InstanceIndex)];
    vec2 position = object.position / vec2(k_viewport_width, k_viewport_height);
    vec2 scale = textureSize(g_font_samplers[nonuniformEXT(object.glyph_index)], 0) / vec2(k_viewport_width, k_viewport_height);
    gl_Position = vec4(k_vertices[gl_VertexIndex] * scale + (position * 2.0f - 1.0f), 0.0f, 1.0f);
    g_fragment.uv = (k_vertices[gl_VertexIndex] + 1.0f) * 0.5f;
    g_fragment.glyph_index = object.glyph_index;
}
