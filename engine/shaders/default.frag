#version 460
#include "lib/common.glsl"
#include "lib/constants.glsl"
#include "lib/gbuffer.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

struct Fragment {
    vec4 colour;
    vec4 normal;
};

layout (location = 0) in vec4 g_position;
layout (location = 1) in vec4 g_normal;
layout (location = 2) in flat uvec2 g_texture_indices;

DECLARE_UBO(0, 0);
layout (binding = 11) coherent buffer AbufferList {
    uint g_abuf_counter;
    uint64_t g_abuf_list[];
};
layout (binding = 12) coherent buffer AbufferData {
    Fragment g_abuf_data[];
};
layout (set = 1, binding = 0) uniform sampler2D g_textures[];

layout (binding = 7) uniform texture2D g_depth_image;

layout (location = 0) out vec4 g_out_albedo;
layout (location = 1) out vec2 g_out_normal;

const float k_alpha_threshold = 0.99f;

vec3 compute_normal(sampler2D map, vec3 position, vec3 normal, vec2 uv) {
    vec3 local_normal = vec3(texture(map, uv).rg, 0.0f) * 2.0f - 1.0f;
    local_normal.z = sqrt(1.0f - dot(local_normal.xy, local_normal.xy));

    vec3 dpos1 = dFdx(position);
    vec3 dpos2 = dFdy(position);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 N = normalize(normal);
    vec3 T = normalize(dpos1 * duv2.y - dpos2 * duv1.y);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * local_normal);
}

uint pack_depth24(float depth) {
    return 0xffffffff - uint(round(depth * float(0xffffff)));
}

bool insert_abuffer(float alpha, vec3 colour, vec3 normal) {
    // Manual depth test, avoid inserting fragments blocked behind any opaque fragments.
    // TODO: Would still be beneficial to sort objects front-to-back CPU-side in this case.
    float opaque_depth = texelFetch(g_depth_image, ivec2(gl_FragCoord.xy), 0).r;
    if (gl_FragCoord.z < opaque_depth) {
        discard;
    }

    uint counter = atomicAdd(g_abuf_counter, 1);
    uint next_index = counter + 2560 * 1440;
    memoryBarrier();

    // Create a packed list entry containing the next element index, as well as the depth and alpha for the fragment.
    // The highest bytes contain the depth to allow sorting.
    uint alpha_unorm = uint(round(alpha * 255.0f));
    uint alpha_depth = (pack_depth24(gl_FragCoord.z) << 8u) | alpha_unorm;
    uint64_t list_entry = pack64(uvec2(next_index, alpha_depth));

    // Try to insert the fragment into the linked list, maintaining
    uint index = k_viewport_width * uint(gl_FragCoord.y) + uint(gl_FragCoord.x);
    bool success = false;
    float accumulated_alpha = 0.0f;
    for (uint i = 0; i < k_max_abuffer_depth; i++) {
        uint64_t old_entry = atomicMax(g_abuf_list[index], list_entry);
        if (old_entry == 0) {
            // Reached the end of the list, can insert here.
            success = true;
            break;
        }

        if (old_entry > list_entry) {
            // atomicMax was a no-op.
            index = uint(old_entry);
            if (!success) {
                float current_alpha = float(unpack32(old_entry).y & 0xff) / 255.0f;
                accumulated_alpha += mix(current_alpha, 0.0f, accumulated_alpha);
                if (accumulated_alpha > k_alpha_threshold) {
                    break;
                }
            }
        } else {
            index = uint(list_entry);
            list_entry = old_entry;
            success = true;
        }
    }

    if (success) {
        g_abuf_data[counter].colour = vec4(colour, 0.0f);
        g_abuf_data[counter].normal = vec4(normal, 0.0f);
    }
    return success;
}

void main() {
    // TODO: Fast path (separate shader) for fully opaque objects.
    vec2 uv = vec2(g_position.w, g_normal.w);
    vec4 albedo = texture(g_textures[g_texture_indices.x], uv);
    if (albedo.a < 1.0f - k_alpha_threshold) {
        // Below threshold, discard fragment completely. Demote to helper invocation as normal computation requires
        // derivatives.
        demote;
    }

    vec3 normal = compute_normal(g_textures[g_texture_indices.y], g_position.xyz, g_normal.xyz, uv);
    if (albedo.a < 1.0f && insert_abuffer(albedo.a, albedo.rgb, normal)) {
        // Success writing to A-buffer, can discard fragment.
        discard;
    }

    // Else, write to the G-buffer, either the fragment is opaque or the A-buffer for the current pixel position is full.
    g_out_albedo = vec4(albedo.rgb, 1.0f);
    g_out_normal = encode_normal(normal);
}
