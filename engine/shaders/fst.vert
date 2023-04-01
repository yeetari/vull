#version 460

layout (location = 0) out vec2 g_out_uv;

void main() {
    g_out_uv = vec2((gl_VertexIndex << 1u) & 2u, gl_VertexIndex & 2u);
    gl_Position = vec4(g_out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
