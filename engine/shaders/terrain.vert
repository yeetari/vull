#version 460

layout (location = 0) in vec2 g_position;

layout (binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
    float terrain_size;
} g_ubo;
layout (binding = 4) uniform sampler2D g_height_map;
layout (push_constant) uniform PushConstant {
    vec3 g_translation;
    float g_size;
};

layout (location = 0) out FragmentData {
    vec3 world_position;
} g_fragment;

void main() {
    g_fragment.world_position = vec3(g_position.x, 0.0f, g_position.y) * (g_size * 2.0f) + g_translation - vec3(g_size, 0.0f, g_size);
    g_fragment.world_position.y += textureLod(g_height_map, g_fragment.world_position.xz / g_ubo.terrain_size, 0.0f).r;
    gl_Position = g_ubo.proj * g_ubo.view * vec4(g_fragment.world_position, 1.0f);
}
