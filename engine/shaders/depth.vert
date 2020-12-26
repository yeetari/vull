#version 450

layout (location = 0) in vec3 position;
layout (set = 0, binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 transform;
    vec3 camera_position;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.transform * vec4(position, 1.0);
}
