layout (constant_id = 0) const uint TILE_SIZE = 32;
layout (constant_id = 1) const uint MAX_LIGHTS_PER_TILE = 400;
layout (constant_id = 2) const uint ROW_TILE_COUNT = 0;
layout (constant_id = 3) const uint VIEWPORT_WIDTH = 0;
layout (constant_id = 4) const uint VIEWPORT_HEIGHT = 0;

struct LightVisibility {
    uint count;
    uint indices[MAX_LIGHTS_PER_TILE];
};
struct PointLight {
    vec3 position;
    float radius;
    vec3 colour;
};
