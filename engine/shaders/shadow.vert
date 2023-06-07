#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;

DECLARE_UBO(0, 0);
DECLARE_DRAW_BUFFER(0, 3);
DECLARE_OBJECT_BUFFER(0, 1);

layout (push_constant) uniform CascadeIndex {
    uint g_cascade_index;
};

void main() {
    Object object = g_objects[g_draws[gl_DrawID].object_index];
    gl_Position = g_shadow_info.cascade_matrices[g_cascade_index] * object.transform * vec4(g_position, 1.0f);
}
