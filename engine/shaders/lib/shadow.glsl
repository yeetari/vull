#ifndef SHADOW_H
#define SHADOW_H

#include "shadow_info.glsl"

const mat4 k_shadow_bias = mat4(
    0.5f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.0f, 1.0f
);

uint choose_cascade(vec4 view_position, uint cascade_count, ShadowInfo info) {
    for (uint i = 0; i < cascade_count; i++) {
        if (abs(view_position.z) < info.cascade_split_depths[i]) {
            return i;
        }
    }
    return cascade_count - 1;
}

float compute_shadow(vec4 world_position, mat4 view, sampler2DArrayShadow map, ShadowInfo info) {
    uint cascade_count = textureSize(map, 0).z;
    uint cascade_index = choose_cascade(view * world_position, cascade_count, info);
    vec4 projected = k_shadow_bias * info.cascade_matrices[cascade_index] * world_position;
    return texture(map, vec4(projected.xy, cascade_index, projected.z)).r;
}

#endif
