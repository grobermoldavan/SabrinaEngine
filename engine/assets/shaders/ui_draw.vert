#version 450

struct Vertex
{
    vec2 position;
    vec2 uv;
    uint coloringIndex;
};

struct Coloring
{
    vec4 tint;
    int mode;
};

layout(        set = 0, binding = 0) readonly buffer MvpMatrix  { mat4 mvpMatrix; };
layout(std140, set = 0, binding = 1) readonly buffer Vertices   { Vertex vertices[]; };
layout(std140, set = 0, binding = 2) readonly buffer Colorings  { Coloring colorings[]; };

layout(        set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(        set = 1, binding = 1) uniform DrawData { uint firstVertexIndex; };

layout (location = 0) out vec2 outUv;
layout (location = 1) out uint outColoringIndex;

void main()
{
    const Vertex vertex = vertices[gl_VertexIndex + firstVertexIndex];
    outUv               = vertex.uv;
    outColoringIndex    = vertex.coloringIndex;
    gl_Position         = mvpMatrix * vec4(vertex.position, 1, 1);
}
