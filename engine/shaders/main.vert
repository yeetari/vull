#version 460

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;

layout (binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;

layout (location = 0) out FragmentData {
    vec3 normal;
    vec3 world_position;
} g_fragment;

void main() {
    g_fragment.normal = g_normal;
    g_fragment.world_position = g_position;
    gl_Position = g_ubo.proj * g_ubo.view * vec4(g_position, 1.0f);
}
