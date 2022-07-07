#version 450

struct Instance
{
    float x;        // bottom left
    float y;        // bottom left
    float width;
    float height;
    float xUv1;     // uv at bottom left
    float yUv1;     // uv at bottom left
    float xUv2;     // uv at top right
    float yUv2;     // uv at top right
};

layout(        set = 0, binding = 0) readonly buffer MvpMatrix { mat4 mvpMatrix; };

layout(        set = 1, binding = 0) uniform sampler2D renderAtlas;
layout(std140, set = 1, binding = 1) readonly buffer Instances { Instance instances[]; };

layout (location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    const float val = texture(renderAtlas, inUv).r;
    outColor = vec4(val, val, val, val);
}
