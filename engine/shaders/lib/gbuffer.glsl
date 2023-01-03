#ifndef GBUFFER_H
#define GBUFFER_H

vec2 sign_non_zero(vec2 v) {
	return vec2(v.x >= 0.0f ? 1.0f : -1.0f, v.y >= 0.0f ? 1.0f : -1.0f);
}

vec3 decode_normal(vec2 encoded) {
    vec3 normal = vec3(encoded, 1.0f - abs(encoded.x) - abs(encoded.y));
    normal.xy = normal.z >= 0.0f ? normal.xy : sign_non_zero(normal.xy) - sign_non_zero(normal.xy) * abs(normal.yx);
    return normalize(normal);
}

vec2 encode_normal(vec3 normal) {
    vec2 octa = normal.xy / (abs(normal.x) + abs(normal.y) + abs(normal.z));
    return normal.z >= 0.0f ? octa : sign_non_zero(octa) - abs(octa.yx) * sign_non_zero(octa);
}

#endif
