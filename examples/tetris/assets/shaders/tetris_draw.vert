#version 450

struct InputVertex
{
    vec3 positionLS;
    vec2 uv;
};

struct InputIndex
{
    uint index;
};

struct InputInstance
{
    mat4x4 transformWS;
    vec4 tint;
};

layout (set = 0, binding = 0) uniform FrameData
{
    mat4x4 viewProjection;
} se_frameData;

layout(std140, set = 1, binding = 0) readonly buffer InputVertices
{
    InputVertex vertices[];
} se_inputVertices;

layout(std140, set = 1, binding = 1) readonly buffer InputIndices
{
    InputIndex indices[];
} se_inputIndices;

layout(std140, set = 1, binding = 2) readonly buffer InputInstances
{
    InputInstance instance[];
} se_inputInstances;

layout (location = 0) out vec2 outUv;
layout (location = 1) out vec4 outTint;

void main()
{
    InputInstance instance = se_inputInstances.instance[gl_InstanceIndex];
    InputVertex vert = se_inputVertices.vertices[se_inputIndices.indices[gl_VertexIndex].index];
    gl_Position = se_frameData.viewProjection * instance.transformWS * vec4(vert.positionLS, 1);
    outUv = vert.uv;
    outTint = instance.tint;
}
