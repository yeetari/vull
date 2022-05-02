#version 460

layout (location = 0) in vec3 g_position;

layout (binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 sun_matrices[4];
    vec3 camera_position;
} g_ubo;

layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
    uint g_normal_index;
    uint g_cascade_index;
};

void main() {
    gl_Position = g_ubo.sun_matrices[g_cascade_index] * g_transform * vec4(g_position, 1.0f);
}
