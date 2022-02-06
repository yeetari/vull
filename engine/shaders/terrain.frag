#version 460
#include "common.glsl"

layout (early_fragment_tests) in;
layout (location = 0) in FragmentData {
    vec3 world_position;
} g_fragment;

layout (binding = 0) readonly uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    vec3 camera_position;
    float terrain_size;
} g_ubo;
layout (binding = 1, std430) readonly buffer Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (binding = 2) readonly buffer LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (binding = 4) uniform sampler2D g_height_map;

layout (location = 0) out vec4 g_out_colour;

const vec3 k_fog_colour = vec3(0.47f, 0.5f, 0.67f);
vec3 g_normal;

float fog(float density) {
    float distance = distance(g_fragment.world_position, g_ubo.camera_position);
    float d = density * distance;
    return 1.0f - clamp(exp2(d * d * -1.442695f), 0.0f, 1.0f);
}

float attenuate(vec3 position, float radius) {
    float distance = distance(position, g_fragment.world_position);
    return clamp(1.0f - distance * distance / (radius * radius), 0.0f, 1.0f);
}

vec3 dir_light(vec3 albedo, vec3 colour, vec3 direction) {
    vec3 light_dir = normalize(-direction);
    return max(dot(g_normal, light_dir), 0.0f) * albedo * colour;
}

vec3 point_light(vec3 albedo, PointLight light) {
    vec3 light_dir = normalize(light.position - g_fragment.world_position);
    return max(dot(g_normal, light_dir), 0.0f) * albedo * attenuate(light.position, light.radius) * light.colour;
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;

    float colour_height = max((g_fragment.world_position.y / 100.0f) - 0.25f, 0.0f);
    vec3 albedo = mix(vec3(0.15f, 0.35f, 0.08f), vec3(0.5f, 0.5f, 0.8f), colour_height);

    vec2 tex_coord = g_fragment.world_position.xz / g_ubo.terrain_size;
    float texel_size = 1.0f / g_ubo.terrain_size;
    float z0 = texture(g_height_map, tex_coord + vec2(-texel_size, -texel_size)).r;
    float z1 = texture(g_height_map, tex_coord + vec2(0.0f, -texel_size)).r;
    float z2 = texture(g_height_map, tex_coord + vec2(texel_size, -texel_size)).r;
    float z3 = texture(g_height_map, tex_coord + vec2(-texel_size, 0.0f)).r;
    float z4 = texture(g_height_map, tex_coord + vec2(texel_size, 0.0f)).r;
    float z5 = texture(g_height_map, tex_coord + vec2(-texel_size, texel_size)).r;
    float z6 = texture(g_height_map, tex_coord + vec2(0.0f, texel_size)).r;
    float z7 = texture(g_height_map, tex_coord + vec2(texel_size, texel_size)).r;
    g_normal = normalize(vec3(z0 + 2.0f*z3 + z5 - z2 - 2.0f*z4 - z7, 20.0f, z0 + 2.0f*z1 + z2 - z5 - 2.0f*z6 - z7));

    vec3 illuminance = albedo * 0.1f;
    illuminance += dir_light(albedo, vec3(0.2f), vec3(-0.2f, -1.0f, -0.3f));
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        illuminance += point_light(albedo, light);
    }
    g_out_colour = vec4(mix(illuminance, k_fog_colour, fog(0.0001f)), 1.0f);
}
