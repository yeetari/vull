#version 450
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;
layout (location = 0) in FragmentData {
    vec3 position;
    vec3 normal;
    vec2 uv;
} g_fragment;

layout (binding = 0, std430) readonly buffer Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (binding = 1) readonly buffer LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (binding = 2) uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;
layout (binding = 3) uniform sampler2D g_textures[];
layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
};

layout (location = 0) out vec4 g_out_colour;

float attenuate(PointLight light) {
    float distance = distance(light.position, g_fragment.position);
    return clamp(1.0f - distance * distance / (light.radius * light.radius), 0.0f, 1.0f);
}

vec3 dir_light(vec3 albedo, vec3 direction) {
    vec3 light_dir = normalize(-direction);
    vec3 view_dir = normalize(g_ubo.camera_position - g_fragment.position);
    vec3 half_dir = normalize(light_dir + view_dir);
    vec3 diffuse = max(dot(g_fragment.normal, light_dir), 0.0f) * albedo;
    vec3 specular = pow(max(dot(g_fragment.normal, half_dir), 0.0f), 64) * vec3(0.3f);
    return (diffuse + specular) * vec3(0.2f);
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;
    vec3 albedo = texture(g_textures[g_albedo_index], g_fragment.uv).rgb;
    vec3 illuminance = albedo * 0.05f + dir_light(albedo, vec3(-0.2f, -1.0f, -0.3f));
    vec3 view_dir = normalize(g_ubo.camera_position - g_fragment.position);
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        vec3 light_dir = normalize(light.position - g_fragment.position);
        vec3 half_dir = normalize(light_dir + view_dir);
        vec3 diffuse = max(dot(g_fragment.normal, light_dir), 0.0f) * albedo;
        vec3 specular = pow(max(dot(g_fragment.normal, half_dir), 0.0f), 64.0f) * vec3(0.3f);
        illuminance += (diffuse + specular) * attenuate(light) * light.colour;
    }
    g_out_colour = vec4(illuminance, 1.0f);
}
