#version 450

struct Instance
{
    uint index;
    float x;
    float y;
    float width;
    float height;
};

vec2 quad[6] = vec2[]
(
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);

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

layout (location = 0) out vec2 outUv;

void main()
{
    const Instance instance = instances[gl_InstanceIndex];
    const vec4 rect = renderAtlasRects[instance.index];
    const vec2 uvs[6] = vec2[]
    (
        vec2(rect.x, rect.y),
        vec2(rect.x, rect.w),
        vec2(rect.z, rect.w),
        vec2(rect.x, rect.y),
        vec2(rect.z, rect.w),
        vec2(rect.z, rect.y)
    );
    outUv = uvs[gl_VertexIndex];
    const vec2 vert = vec2(quad[gl_VertexIndex].x * instance.width, quad[gl_VertexIndex].y * instance.height);
    const vec2 base = vec2(instance.x, instance.y);
    gl_Position = mvpMatrix * vec4(base + vert, 3, 1);
}
