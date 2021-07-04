#version 450

layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    mat4 g_proj_view;
};

layout (location = 0) out vec3 normal;

void main() {
    normal = normalize((transpose(inverse(g_transform)) * vec4(g_normal, 0.0f)).xyz);
    gl_Position = g_proj_view * g_transform * vec4(g_position, 1.0f);
}
