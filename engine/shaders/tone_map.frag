#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput g_in_colour;

layout (location = 0) out vec4 g_out_colour;

void main() {
    vec3 colour = subpassLoad(g_in_colour).rgb;
    vec3 tone_mapped = vec3(1.0F) - exp(-colour * 0.5F);
    g_out_colour = vec4(tone_mapped, 1.0F);
}
