#version 450

struct Vertex
{
    vec2 position;
    vec2 uv;
    uint colorIndex;
};

struct Color
{
    vec4 rChannel;
    vec4 gChannel;
    vec4 bChannel;
    vec4 mask;
    float minDivider;
    float maxDivider;
};

layout(        set = 0, binding = 0) readonly buffer MvpMatrix { mat4 mvpMatrix; };

layout(        set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(std140, set = 1, binding = 1) readonly buffer Colors { Color colors[]; };
layout(std140, set = 1, binding = 2) readonly buffer Vertices { Vertex vertices[]; };

layout (location = 0) out vec2 outUv;
layout (location = 1) out uint outColorIndex;

void main()
{
    const Vertex vertex = vertices[gl_VertexIndex];
    outUv = vertex.uv;
    outColorIndex = vertex.colorIndex;
    gl_Position = mvpMatrix * vec4(vertex.position, 1, 1);
}
