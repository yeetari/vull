#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in FragmentData {
    vec2 uv;
    flat uint glyph_index;
} g_fragment;

layout (binding = 6) uniform sampler2D g_font_samplers[];
layout (location = 0) out vec4 g_out_colour;

void main() {
    float distance = 1.0f - texture(g_font_samplers[nonuniformEXT(g_fragment.glyph_index)], g_fragment.uv).r;
    float alpha = 1.0f - smoothstep(0.5f, 0.6f, distance);
    g_out_colour = vec4(vec3(0.0f), alpha);
}
