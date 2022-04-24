#version 460

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

layout (binding = 0) uniform UniformBuffer {
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
    mat4 normal_matrix = transpose(inverse(g_transform));
    vec4 world_position = g_transform * vec4(g_position, 1.0f);
    g_fragment.position = world_position.xyz;
    g_fragment.normal = (normal_matrix * vec4(g_normal, 0.0f)).xyz;
    g_fragment.uv = g_uv;
    gl_Position = g_ubo.proj * g_ubo.view * world_position;
}
