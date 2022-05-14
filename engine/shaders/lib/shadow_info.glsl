#ifndef SHADOW_INFO_H
#define SHADOW_INFO_H

const uint k_max_cascade_count = 8;
struct ShadowInfo {
    mat4 cascade_matrices[k_max_cascade_count];
    float cascade_split_depths[k_max_cascade_count];
};

#endif
