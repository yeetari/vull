#version 450

layout (location = 0) in vec3 normal;

layout (location = 0) out vec4 g_out_colour;

void main() {
    g_out_colour = vec4(normalize(normal), 1.0f);
}
