#version 450

struct Vertex
{
    vec2 position;
    vec2 uv;
};

layout(        set = 0, binding = 0) readonly buffer MvpMatrix { mat4 mvpMatrix; };

layout(        set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(std140, set = 1, binding = 1) readonly buffer Colors
{
    vec4 rChannel;
    vec4 gChannel;
    vec4 bChannel;
    vec4 aChannel;
    vec4 mask;
    float divider;
};
layout(std140, set = 1, binding = 2) readonly buffer Vertices { Vertex vertices[]; };

layout (location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    const vec4 c = texture(renderAtlas, inUv);
    outColor = ((mask.r * c.r * rChannel) + (mask.g * c.g * gChannel) + (mask.b * c.b * bChannel) + (mask.a * c.a * aChannel)) / divider;
}
