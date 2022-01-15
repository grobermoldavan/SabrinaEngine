#version 450

struct Vertex
{
    vec4 position;
    vec4 normal;
};

layout (set = 0, binding = 0) uniform FrameData
{
    mat4 viewProjection;
    float time;
    float isoLevel;
};

layout (set = 1, binding = 1) buffer ChunkGeometry
{
    Vertex geometry[];
    // One cube is 8 triangles - 8 * 3 vertices.
    // Full chunk consists of (CHUNK_BOUNDS_X - 1) * (CHUNK_BOUNDS_Y - 1) * (CHUNK_BOUNDS_Z - 1) * 8 * 3 vertices.
};


void main()
{
    Vertex vert = geometry[gl_VertexIndex];
    gl_Position = viewProjection * vert.position;
}
