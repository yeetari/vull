#version 450
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;
layout (location = 0) in vec3 g_position;
layout (location = 1) in vec3 g_normal;
layout (location = 2) in vec2 g_uv;

layout (set = 0, binding = 0, std140) readonly buffer Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (set = 0, binding = 1) readonly buffer LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (set = 0, binding = 2) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;
layout (set = 0, binding = 3) uniform sampler2D g_textures[];
layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
};

layout (location = 0) out vec4 g_out_colour;

float attenuate(PointLight light) {
    float distance = distance(light.position, g_position);
    return clamp(1.0 - distance * distance / (light.radius * light.radius), 0.0, 1.0);
}

vec3 dir_light(vec3 albedo, vec3 direction) {
    vec3 light_dir = normalize(-direction);
    vec3 view_dir = normalize(g_ubo.camera_position - g_position);
    vec3 half_dir = normalize(light_dir + view_dir);
    vec3 diffuse = max(dot(g_normal, light_dir), 0.0) * albedo;
    vec3 specular = pow(max(dot(g_normal, half_dir), 0.0), 64) * vec3(0.3);
    return (diffuse + specular) * vec3(0.2F);
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;
    vec3 albedo = texture(g_textures[g_albedo_index], g_uv).rgb;
    vec3 illuminance = albedo * 0.05 + dir_light(albedo, vec3(-0.2F, -1.0F, -0.3F));
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
