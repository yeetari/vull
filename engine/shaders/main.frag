#version 460
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout (early_fragment_tests) in;
layout (location = 0) in FragmentData {
    vec3 position;
    vec3 normal;
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
layout (binding = 4) uniform sampler g_albedo_sampler;
layout (binding = 5) uniform sampler g_normal_sampler;
layout (binding = 6) uniform texture2D g_textures[];

layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
    uint g_normal_index;
};

layout (location = 0) out vec4 g_out_colour;

vec3 calc_light(vec3 albedo, vec3 colour, vec3 direction, vec3 normal, vec3 view, float attenuation) {
    vec3 diffuse = max(dot(normal, direction), 0.0f) * albedo * colour;
    vec3 reflection = reflect(-direction, normal);
    vec3 halfway = normalize(direction + view);
    vec3 specular = pow(max(dot(normal, halfway), 0.0f), 64.0f) * colour;
    return (diffuse + specular) * attenuation;
}

vec3 calc_normal(texture2D map, vec3 surface_normal, vec3 world_position, vec2 uv) {
    vec3 local_normal = vec3(texture(sampler2D(map, g_normal_sampler), uv).rg, 0.0f) * 2.0f - 1.0f;
    local_normal.z = sqrt(1.0f - dot(local_normal.xy, local_normal.xy));

    vec3 dpos1 = dFdx(world_position);
    vec3 dpos2 = dFdy(world_position);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 N = normalize(surface_normal);
    vec3 T = normalize(dpos1 * duv2.y - dpos2 * duv1.y);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * local_normal);
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;
    vec3 albedo = texture(sampler2D(g_textures[nonuniformEXT(g_albedo_index)], g_albedo_sampler), g_fragment.uv).rgb;
    vec3 normal = calc_normal(g_textures[nonuniformEXT(g_normal_index)], g_fragment.normal, g_fragment.position, g_fragment.uv);

    vec3 illuminance = albedo * 0.05f;
    vec3 view = normalize(g_ubo.camera_position - g_fragment.position);
    illuminance += calc_light(albedo, vec3(0.5f), vec3(0.45f, 0.35f, 0.8f), normal, view, 1.0f);
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        vec3 direction = normalize(light.position - g_fragment.position);
        float dist = distance(light.position, g_fragment.position);
        float attenuation = clamp(1.0f - dist * dist / (light.radius * light.radius), 0.0f, 1.0f);
        illuminance += calc_light(albedo, light.colour, direction, normal, view, attenuation);
    }
    float fog = fog_factor(0.005f, distance(g_fragment.position, g_ubo.camera_position));
    g_out_colour = vec4(mix(illuminance, vec3(0.47f, 0.5f, 0.67f), fog), 1.0f);
}
