#version 460
#include "lib/common.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_position;

DECLARE_UBO(0);
layout (binding = 3, scalar) readonly buffer DrawBuffer {
    DrawCmd g_draws[];
};

layout (push_constant) uniform CascadeIndex {
    uint g_cascade_index;
};

void main() {
    DrawCmd draw = g_draws[gl_DrawID];
    gl_Position = g_shadow_info.cascade_matrices[g_cascade_index] * draw.transform * vec4(g_position, 1.0f);
}
