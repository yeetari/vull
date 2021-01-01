#version 450
#include "common.glsl"

layout (early_fragment_tests) in;
layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;

layout (std140, set = 0, binding = 0) buffer readonly Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (set = 0, binding = 1) buffer readonly LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (set = 1, binding = 0) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 transform;
    vec3 camera_position;
} g_ubo;

layout (location = 0) out vec4 g_out_colour;

float attenuate(PointLight light) {
    float distance = distance(light.position, g_position);
    return clamp(1.0 - distance * distance / (light.radius * light.radius), 0.0, 1.0);
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / TILE_SIZE);
    uint tile_index = tile_id.y * ROW_TILE_COUNT + tile_id.x;
    vec3 albedo = vec3(1);
    vec3 illuminance = albedo * 0.05;
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        vec3 light_dir = normalize(light.position - g_position);
        vec3 view_dir = normalize(g_ubo.camera_position - g_position);
        vec3 half_dir = normalize(light_dir + view_dir);
        vec3 diffuse = max(dot(g_normal, light_dir), 0.0) * albedo;
        vec3 specular = pow(max(dot(g_normal, half_dir), 0.0), 64) * vec3(0.3);
        illuminance += (diffuse + specular) * attenuate(light) * light.colour;
    }
    g_out_colour = vec4(illuminance, 1);
}
