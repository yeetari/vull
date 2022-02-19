#version 460
#include "ui.glsl"

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) in FragmentData {
    vec2 uv;
    flat uint object_id;
} g_fragment;

layout (binding = 0, scalar) readonly buffer UiData {
    UiObject g_objects[];
};
layout (binding = 1) uniform sampler2D g_font_samplers[];

layout (location = 0) out vec4 g_out_colour;

void main() {
    UiObject object = g_objects[nonuniformEXT(g_fragment.object_id)];
    switch (object.type) {
    case 0:
        g_out_colour = object.colour;
        break;
    case 1:
        g_out_colour = vec4(0.0f);
        if (g_fragment.uv.x <= 0.001f || g_fragment.uv.x >= 0.999f || g_fragment.uv.y <= 0.002f || g_fragment.uv.y >= 0.998f) {
            g_out_colour = object.colour;
        }
        break;
    case 2:
        float distance = 1.0f - textureLod(g_font_samplers[nonuniformEXT(object.glyph_index)], g_fragment.uv, 0).r;
        float alpha = 1.0f - smoothstep(0.5f, 0.6f, distance);
        float outline_alpha = 1.0f - smoothstep(0.6f, 0.7f, distance);
        float overall_alpha = alpha + (1.0f - alpha) * outline_alpha;
        vec3 overall_colour = mix(vec3(0.0f), object.colour.rgb, alpha / overall_alpha);
        g_out_colour = vec4(overall_colour, overall_alpha);
        break;
    }
}
