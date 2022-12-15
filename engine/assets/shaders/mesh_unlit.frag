#version 450

struct InputInstanceData
{
    mat4x4 mvp;
};

layout (std140, set = 0, binding = 0) readonly buffer Positions { vec4 positions[]; };
layout (std140, set = 0, binding = 1) readonly buffer Uvs { vec4 uvs[]; };
layout (std430, set = 0, binding = 2) readonly buffer Indices { uint indices[]; };
layout (std140, set = 0, binding = 3) readonly buffer InputInstances { InputInstanceData instances[]; };
layout (        set = 0, binding = 4) uniform sampler2D colorTexture;

layout (location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(colorTexture, inUv);
}
