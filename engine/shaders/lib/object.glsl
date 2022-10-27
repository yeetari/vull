#ifndef OBJECT_H
#define OBJECT_H

struct FragmentData {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout (push_constant) uniform ObjectData {
    mat4 g_transform;
    uint g_albedo_index;
    uint g_normal_index;
    uint g_cascade_index;
};

mat4 object_transform() {
    return g_transform;
}

uint object_albedo_index() {
    return g_albedo_index;
}

uint object_normal_index() {
    return g_normal_index;
}

uint cascade_index() {
    return g_cascade_index;
}

#ifdef FRAG_SHADER
vec3 compute_normal(sampler2D map, FragmentData fragment) {
    vec3 local_normal = vec3(texture(map, fragment.uv).rg, 0.0f) * 2.0f - 1.0f;
    local_normal.z = sqrt(1.0f - dot(local_normal.xy, local_normal.xy));

    vec3 dpos1 = dFdx(fragment.position);
    vec3 dpos2 = dFdy(fragment.position);
    vec2 duv1 = dFdx(fragment.uv);
    vec2 duv2 = dFdy(fragment.uv);

    vec3 N = normalize(fragment.normal);
    vec3 T = normalize(dpos1 * duv2.y - dpos2 * duv1.y);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * local_normal);
}
#endif

#endif
