#version 450

struct Instance
{
    uint index;
    float x;
    float y;
    float width;
    float height;
};

layout(set = 0, binding = 0) readonly buffer MvpMatrix
{
    mat4 mvpMatrix;
};

layout(set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(std140, set = 1, binding = 1) readonly buffer RenderAtlasRects
{
    vec4 renderAtlasRects[];
};

layout(std140, set = 2, binding = 0) readonly buffer Instances
{
    Instance instances[];
};

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    const float val = texture(renderAtlas, inUv).x;
    //outColor = val > 0.5 ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 0);
    outColor = vec4(val, val, val, val);
}
