DECLARE_UBO(0, 0);
layout (binding = 1) uniform sampler2D g_height_map;

layout (push_constant, scalar) uniform PushConstant {
    vec2 g_translation;
    uint g_terrain_size;
    uint g_chunk_size;
    float g_morph_const_z;
    float g_morph_const_w;
};

float sample_height(vec2 world_point) {
    vec2 uv = world_point / textureSize(g_height_map, 0);
    float height = textureLod(g_height_map, uv, 0.0f).r;

//    height *= 1024.0f;
//    height += 64.0f * textureLod(g_height_map, 16.0f * uv, 0.0f).r;
//    height /= 1024.0f;

    return height * 2.0f;
//    return height * g_terrain_size / 200.0f;
}
