#version 450

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

layout (binding = 2) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;
layout (push_constant) uniform ObjectData {
    mat4 g_transform;
};

layout (location = 0) out FragmentData {
    vec3 position;
    vec3 normal;
    vec2 uv;
} g_fragment;

void main() {
    mat4 trans_inv_transform = transpose(inverse(g_transform));
    g_fragment.position = vec3(g_transform * vec4(g_position, 1.0f));
    g_fragment.normal = normalize((trans_inv_transform * vec4(g_normal, 0.0f)).xyz);
    g_fragment.uv = g_uv;
    gl_Position = g_ubo.proj * g_ubo.view * g_transform * vec4(g_position, 1.0f);
}
