#version 450

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;

layout (set = 1, binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 transform;
    vec3 camera_position;
} g_ubo;

layout (location = 0) out vec3 g_out_position;
layout (location = 1) out vec3 g_out_normal;

void main() {
    mat4 trans_inv_transform = transpose(inverse(g_ubo.transform));
    g_out_position = vec3(g_ubo.transform * vec4(g_position, 1.0));
    g_out_normal = normalize((trans_inv_transform * vec4(g_normal, 0.0)).xyz);
    gl_Position = g_ubo.proj * g_ubo.view * g_ubo.transform * vec4(g_position, 1.0);
}
