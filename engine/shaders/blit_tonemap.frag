#version 460
#include "lib/common.glsl"

layout (location = 0) in vec2 g_uv;
layout (binding = 7) uniform sampler2D g_hdr_image;
layout (location = 0) out vec4 g_output_image;

layout (push_constant) uniform PushConstants {
    float g_exposure;
};

vec3 vignette(const vec3 colour) {
    vec2 uv = g_uv * (1.0f - g_uv.yx);
    float factor = uv.x * uv.y * 15.0f;
    return colour * pow(factor, 0.15f);
}

void main() {
    vec3 colour = texture(g_hdr_image, g_uv).rgb;
    colour = vec3(1.0f) - exp(-colour * g_exposure);
    colour = vignette(colour);
    g_output_image = vec4(colour, 1.0f);
}
