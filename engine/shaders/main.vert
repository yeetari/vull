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
    uint g_albedo_index;
};

layout (location = 0) out FragmentData {
    vec3 normal;
    vec3 world_position;
    vec2 uv;
} g_fragment;

void main() {
    vec4 world_position = g_transform * vec4(g_position, 1.0f);
    g_fragment.normal = normalize((transpose(inverse(g_transform)) * vec4(g_normal, 0.0f)).xyz);
    g_fragment.world_position = world_position.xyz;
    g_fragment.uv = g_uv;
    gl_Position = g_ubo.proj * g_ubo.view * world_position;
}
