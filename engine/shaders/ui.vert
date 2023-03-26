#version 460

layout (location = 0) in vec2 g_position;
layout (location = 1) in vec2 g_uv;
layout (location = 2) in vec4 g_colour;

layout (location = 0) out vec2 g_out_uv;
layout (location = 1) out vec4 g_out_colour;

layout (push_constant) uniform PushConstants {
    vec2 g_viewport;
    uint g_texture_index;
};

void main() {
    g_out_uv = g_uv;
    g_out_colour = g_colour;
    gl_Position = vec4(g_position * (vec2(2.0f) / g_viewport) - 1.0f, 0.0f, 1.0f);
}
