#version 450

layout(location = 0) in vec2 inUv;

layout(set = 0, binding = 0) uniform Input { vec4 color; };

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = color;
}
