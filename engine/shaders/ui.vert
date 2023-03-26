#version 460

layout (constant_id = 0) const float k_viewport_width = 0;
layout (constant_id = 1) const float k_viewport_height = 0;

layout (location = 0) in vec2 g_position;
layout (location = 1) in vec2 g_uv;
layout (location = 2) in vec4 g_colour;

layout (location = 0) out vec2 g_out_uv;
layout (location = 1) out vec4 g_out_colour;

void main() {
    g_out_uv = g_uv;
    g_out_colour = g_colour;
    gl_Position = vec4(g_position * (vec2(2.0f) / vec2(k_viewport_width, k_viewport_height)) - 1.0f, 0.0f, 1.0f);
}
