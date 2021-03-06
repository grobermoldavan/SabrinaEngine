#version 450

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform sampler2D resultImage;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(resultImage, inUv);
}
