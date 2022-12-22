#ifndef COMMON_H
#define COMMON_H

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_null_initializer : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_scalar_block_layout : enable

mat3 adjugate(mat3 mat) {
    return mat3(cross(mat[1], mat[2]), cross(mat[2], mat[0]), cross(mat[0], mat[1]));
}

mat3 adjugate(mat4 mat) {
    return adjugate(mat3(mat));
}

float linearise_depth(float depth, mat4 proj) {
    // proj[3][2] is the near plane for an inverse, reverse depth projection matrix.
    return proj[3][2] / depth;
}

// See the Khronos Data Format Specification v1.3.1 section 13.3.1
vec3 srgb_to_linear(vec3 srgb) {
    vec3 r1 = srgb / 12.92f;
    vec3 r2 = pow((srgb + 0.055f) / 1.055f, vec3(2.4f));
    return mix(r1, r2, greaterThan(srgb, vec3(0.04045f)));
}
vec4 srgb_to_linear(vec4 srgb) {
    return vec4(srgb_to_linear(srgb.rgb), srgb.a);
}

// See the Khronos Data Format Specification v1.3.1 section 13.3.2
vec3 linear_to_srgb(vec3 linear) {
    vec3 r1 = linear * 12.92f;
    vec3 r2 = pow(linear, vec3(1.0f / 2.4f)) * 1.055f - 0.055f;
    return mix(r1, r2, greaterThan(linear, vec3(0.0031308f)));
}
vec4 linear_to_srgb(vec4 linear) {
    return vec4(linear_to_srgb(linear.rgb), linear.a);
}

#endif
