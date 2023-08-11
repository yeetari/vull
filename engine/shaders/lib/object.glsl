#ifndef OBJECT_H
#define OBJECT_H

struct DrawCmd {
    uint index_count;
    uint instance_count;
    uint first_index;
    uint vertex_offset;
    uint first_instance;
    uint object_index;
};

struct Object {
    mat4 transform;
    float center[3];
    float radius;
    uint albedo_index;
    uint normal_index;
    uint index_count;
    uint first_index;
    uint vertex_offset;
};

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

#define DECLARE_DRAW_BUFFER(s, b) \
layout (set = s, binding = b) restrict buffer DrawBuffer { \
    uint g_draw_count; \
    DrawCmd g_draws[]; \
};

#define DECLARE_OBJECT_BUFFER(s, b) \
layout (set = s, binding = b, scalar) restrict readonly buffer ObjectBuffer { \
    Object g_objects[]; \
};

#define DECLARE_VERTEX_BUFFER(s, b) \
layout (set = s, binding = b, scalar) restrict readonly buffer VertexBuffer { \
    Vertex g_vertices[]; \
};

#endif
