#ifndef OBJECT_H
#define OBJECT_H

struct DrawCmd {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
    uint albedo_index;
    uint normal_index;
    mat4 transform;
};

#ifdef FRAG_SHADER
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
#endif

#endif
