#version 450

layout (location = 0) in vec3 g_position;

layout (set = 0, binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;
layout (push_constant) uniform ObjectTransform {
    mat4 g_transform;
};

void main() {
    gl_Position = g_ubo.proj * g_ubo.view * g_transform * vec4(g_position, 1.0);
}
