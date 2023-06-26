#ifndef LIGHTING_H
#define LIGHTING_H

struct PointLight {
    vec3 position;
    float radius;
    vec3 colour;
};

struct LightVisibility {
    uint count;
    uint indices[256];
};

vec3 compute_light(vec3 albedo, vec3 colour, vec3 direction, vec3 normal, vec3 view) {
    vec3 diffuse = max(dot(normal, direction), 0.0f) * albedo * colour;
    vec3 reflection = reflect(-direction, normal);
    vec3 halfway = normalize(direction + view);
    vec3 specular = pow(max(dot(normal, halfway), 0.0f), 64.0f) * colour;
    return diffuse;
}

#endif
