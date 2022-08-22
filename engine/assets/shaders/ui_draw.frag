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

layout(        set = 0, binding = 0) readonly buffer MvpMatrix { mat4 mvpMatrix; };

layout(        set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(std140, set = 1, binding = 1) readonly buffer Colorings { Coloring colorings[]; };
layout(std140, set = 1, binding = 2) readonly buffer Vertices { Vertex vertices[]; };

layout (location = 0) in vec2 inUv;
layout (location = 1) flat in uint inColoringIndex;

layout(location = 0) out vec4 outColor;

const int MODE_TEXT = 0;
const int MODE_SIMPLE = 1;

void main()
{
    const Coloring coloring = colorings[inColoringIndex];
    if (coloring.mode == MODE_TEXT)
    {
        const vec4 sampledColor = texture(renderAtlas, inUv);
        outColor = sampledColor.r * coloring.tint;
    }
    else if (coloring.mode == MODE_SIMPLE)
    {
        outColor = coloring.tint;
    }
}
