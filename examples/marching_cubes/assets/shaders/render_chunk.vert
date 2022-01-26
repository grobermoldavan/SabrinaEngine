#version 450

struct Vertex
{
    vec4 position;
    vec4 normal;
};

layout (std140, set = 0, binding = 0) uniform FrameData
{
    mat4 viewProjection;
    float time;
    float isoLevel;
};

layout (set = 1, binding = 0) buffer ChunkGeometry
{
    Vertex geometry[];
};

layout(location = 0) out vec4 normal;

void main()
{
    Vertex vert = geometry[gl_VertexIndex];
    gl_Position = viewProjection * vert.position;
    normal = vert.normal;
}
