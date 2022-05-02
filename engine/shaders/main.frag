#version 460
#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (early_fragment_tests) in;
layout (location = 0) in FragmentData {
    vec3 position;
    vec3 normal;
    vec2 uv;
} g_fragment;

layout (binding = 0, std430) readonly uniform UniformBuffer {
    mat4 proj;
    mat4 view;
    mat4 sun_matrices[4];
    vec3 camera_position;
    float sun_cascade_split_depths[4];
} g_ubo;
layout (binding = 1, std430) readonly buffer Lights {
    uint g_light_count;
    PointLight g_lights[];
};
layout (binding = 2) readonly buffer LightVisibilities {
    LightVisibility g_light_visibilities[];
};
layout (binding = 4) uniform sampler2DArray g_shadow_map;
layout (binding = 5) uniform sampler g_albedo_sampler;
layout (binding = 6) uniform sampler g_normal_sampler;
layout (binding = 7) uniform texture2D g_textures[];

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

const mat4 k_shadow_bias = mat4(
    0.5f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.0f, 1.0f
);

uint choose_cascade(vec4 world_position) {
    vec4 view_position = g_ubo.view * world_position;
    for (uint i = 0; i < 4; i++) {
        if (abs(view_position.z) < g_ubo.sun_cascade_split_depths[i]) {
            return i;
        }
    }
    return 3;
}

float shadow_factor(vec4 world_position, sampler2DArray shadow_map) {
    uint cascade_index = choose_cascade(world_position);
    vec4 projected = k_shadow_bias * g_ubo.sun_matrices[cascade_index] * world_position;

    float shadow = 0.0f;
    float texel_size = 1.0f / textureSize(shadow_map, 0).x;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closest_depth = texture(shadow_map, vec3(projected.xy + vec2(x, y) * texel_size, cascade_index)).r;
            shadow += closest_depth < projected.z ? 0.0f : 1.0f;
        }
    }
    return shadow / 9.0f;
}

void main() {
    ivec2 tile_id = ivec2(gl_FragCoord.xy / k_tile_size);
    uint tile_index = tile_id.y * k_row_tile_count + tile_id.x;
    vec3 albedo = texture(sampler2D(g_textures[nonuniformEXT(g_albedo_index)], g_albedo_sampler), g_fragment.uv).rgb;
    vec3 normal = calc_normal(g_textures[nonuniformEXT(g_normal_index)], g_fragment.normal, g_fragment.position, g_fragment.uv);

    vec3 illuminance = albedo * 0.02f;
    vec3 view = normalize(g_ubo.camera_position - g_fragment.position.xyz);

    // Sun.
    const vec3 sun_direction = vec3(0.6f, 0.6f, -0.6f);
    float sun_shadow = shadow_factor(vec4(g_fragment.position, 1.0f), g_shadow_map);
    illuminance += calc_light(albedo, vec3(1.0f), sun_direction, normal, view, 1.0f) * sun_shadow;

    // Point lights.
    for (uint i = 0; i < g_light_visibilities[tile_index].count; i++) {
        PointLight light = g_lights[g_light_visibilities[tile_index].indices[i]];
        vec3 direction = normalize(light.position - g_fragment.position);
        float dist = distance(light.position, g_fragment.position);
        float attenuation = clamp(1.0f - dist * dist / (light.radius * light.radius), 0.0f, 1.0f);
        illuminance += calc_light(albedo, light.colour, direction, normal, view, attenuation);
    }

    // Final output with fog blended in.
    float fog = fog_factor(0.004f, distance(g_fragment.position, g_ubo.camera_position));
    g_out_colour = vec4(mix(illuminance, vec3(0.47f, 0.5f, 0.67f), fog), 1.0f);
}
