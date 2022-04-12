#version 460
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;
layout (location = 0) in FragmentData {
    vec3 normal;
    vec3 world_position;
    vec2 uv;
} g_fragment;

layout (binding = 0) readonly uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
} g_ubo;
layout (binding = 1, std430) readonly buffer Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (binding = 2) readonly buffer LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (binding = 4) uniform sampler g_texture_sampler;
layout (binding = 5) uniform texture2D g_textures[];

layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
};

layout (location = 0) out vec4 g_out_colour;

float attenuate(vec3 position, float radius) {
    float distance = distance(position, g_fragment.world_position);
    return clamp(1.0f - distance * distance / (radius * radius), 0.0f, 1.0f);
}

vec3 dir_light(vec3 albedo, vec3 colour, vec3 direction) {
    vec3 light_dir = normalize(-direction);
    return max(dot(g_fragment.normal, light_dir), 0.0f) * albedo * colour;
}

vec3 point_light(vec3 albedo, PointLight light) {
    vec3 light_dir = normalize(light.position - g_fragment.world_position);
    return max(dot(g_fragment.normal, light_dir), 0.0f) * albedo * attenuate(light.position, light.radius) * light.colour;
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;
    vec3 albedo = texture(sampler2D(g_textures[nonuniformEXT(g_albedo_index)], g_texture_sampler), g_fragment.uv).rgb;

    vec3 illuminance = albedo * 0.1f;
    illuminance += dir_light(albedo, vec3(0.2f), vec3(-0.2f, -1.0f, -0.3f));
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        illuminance += point_light(albedo, light);
    }
    float fog = fog_factor(0.005f, distance(g_fragment.world_position, g_ubo.camera_position));
    g_out_colour = vec4(mix(illuminance, vec3(0.47f, 0.5f, 0.67f), fog), 1.0f);
}
