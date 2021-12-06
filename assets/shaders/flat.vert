#version 450

struct InputVertex
{
    vec3 positionLS;
    vec2 uv;
};

struct InputInstanceData
{
    mat4x4 transformWS;
};

layout (set = 0, binding = 0) uniform FrameData
{
    mat4x4 viewProjection;
} se_frameData;

layout(std140, set = 1, binding = 0) readonly buffer InputGeometry
{
    InputVertex vertices[];
} se_inputGeometry;

layout(std140, set = 1, binding = 1) readonly buffer InputInstances
{
    InputInstanceData instanceData[];
} se_inputInstances;

layout (location = 0) out vec2 outUv;

void main()
{
    InputInstanceData instanceData = se_inputInstances.instanceData[gl_InstanceIndex];
    InputVertex vert = se_inputGeometry.vertices[gl_VertexIndex];
    gl_Position = se_frameData.viewProjection * instanceData.transformWS * vec4(vert.positionLS, 1);
    outUv = vert.uv;
}