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

layout (location = 0) in vec2 inUv;
layout (location = 1) flat in uint inColorIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    const Color color = colors[inColorIndex];
    const vec4 sampledColor = texture(renderAtlas, inUv);
    const float sampledSum = sampledColor.r + sampledColor.g + sampledColor.b;
    outColor =
        ((color.mask.r * sampledColor.r * color.rChannel) +
         (color.mask.g * sampledColor.g * color.gChannel) +
         (color.mask.b * sampledColor.b * color.bChannel)) /
        clamp(sampledSum, color.minDivider, color.maxDivider);
}
