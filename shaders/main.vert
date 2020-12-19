#version 450

layout (location = 0) in vec3 position;
layout (binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 transform;
} ubo;
layout (location = 0) out vec3 out_position;

void main() {
    out_position = position;
    gl_Position = ubo.proj * ubo.view * ubo.transform * vec4(position, 1.0);
}
