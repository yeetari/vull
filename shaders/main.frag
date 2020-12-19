#version 450

layout (location = 0) in vec3 in_position;
layout (location = 0) out vec4 out_colour;

void main() {
    out_colour = vec4(in_position, 1);
}
