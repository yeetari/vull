#include "shadow_info.glsl"
#extension GL_EXT_scalar_block_layout : enable

#define DECLARE_UBO(b) layout (binding = b, scalar) readonly uniform UniformBuffer { \
    mat4 g_proj; \
    mat4 g_view; \
    vec3 g_view_position; \
    ShadowInfo g_shadow_info; \
 }
