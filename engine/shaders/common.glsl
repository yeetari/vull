layout (constant_id = 0) const uint k_tile_size = 32;
layout (constant_id = 1) const uint k_max_lights_per_tile = 400;
layout (constant_id = 2) const uint k_row_tile_count = 0;
layout (constant_id = 3) const uint k_viewport_width = 0;
layout (constant_id = 4) const uint k_viewport_height = 0;

struct LightVisibility {
    uint count;
    uint indices[k_max_lights_per_tile];
};
struct PointLight {
    vec3 position;
    float radius;
    vec3 colour;
};
