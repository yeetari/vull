#version 450

// TODO: Is this actually faster than binding a buffer?
// TODO: Maybe use indices?
const vec3[] QUAD_VERTICES = vec3[6] (
    vec3(-1.0F, -1.0F, 0.0F),
    vec3(-1.0F, 1.0F, 0.0F),
    vec3(1.0F, 1.0F, 0.0F),
    vec3(-1.0F, -1.0F, 0.0F),
    vec3(1.0F, 1.0F, 0.0F),
    vec3(1.0F, -1.0F, 0.0F)
);

void main() {
    gl_Position = vec4(QUAD_VERTICES[gl_VertexIndex], 1.0F);
}
