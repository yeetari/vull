#version 460
#include "lib/common.glsl"
#include "lib/gbuffer.glsl"
#include "lib/object.glsl"
#include "lib/ubo.glsl"

layout (location = 0) in vec3 g_normal;

layout (location = 0) out vec4 g_out_colour;

void main() {
    // TODO: Base colour from texture.
    vec3 base_colour = vec3(1.0f);
    vec3 illuminance = base_colour * 0.05f;
    illuminance += base_colour * max(dot(g_normal, vec3(0.75f, 0.75f, 0.5f)), 0.0f);
    g_out_colour = vec4(illuminance, 1.0f);
}
