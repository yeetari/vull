#version 460
#include "lib/common.glsl"

layout (location = 0) in vec2 g_uv;
layout (binding = 11) uniform sampler2D g_hdr_image;
layout (location = 0) out vec4 g_output_image;

void main() {
    // TODO: Tonemapping.
    vec3 hdr = texture(g_hdr_image, g_uv).rgb;
    g_output_image = vec4(hdr, 1.0f);
}
