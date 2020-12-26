const int TILE_SIZE = 32;
const uint MAX_LIGHTS_PER_TILE = 400;

struct LightVisibility {
    uint count;
    uint indices[MAX_LIGHTS_PER_TILE];
};
struct PointLight {
    vec3 position;
    float radius;
    vec3 colour;
};
