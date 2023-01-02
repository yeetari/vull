#include "shadow_info.glsl"

#define DECLARE_UBO(s, b)  \
layout (set = s, binding = b, scalar) readonly uniform UniformBuffer { \
    mat4 g_proj; \
    mat4 g_cull_view; \
    mat4 g_view; \
    vec3 g_view_position; \
    uint g_object_count; \
    vec4 g_frustum_planes[4]; \
    ShadowInfo g_shadow_info; \
 }
