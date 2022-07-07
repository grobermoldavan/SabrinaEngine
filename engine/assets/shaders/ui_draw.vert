#version 450

vec2 quad[6] = vec2[]
(
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);

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

layout (location = 0) out vec2 outUv;

void main()
{
    const Instance instance = instances[gl_InstanceIndex];
    const vec2 uvs[6] = vec2[]
    (
        vec2(instance.xUv1, instance.yUv1),
        vec2(instance.xUv1, instance.yUv2),
        vec2(instance.xUv2, instance.yUv2),
        vec2(instance.xUv1, instance.yUv1),
        vec2(instance.xUv2, instance.yUv2),
        vec2(instance.xUv2, instance.yUv1)
    );
    outUv = uvs[gl_VertexIndex];
    const vec2 vert = vec2(quad[gl_VertexIndex].x * instance.width, quad[gl_VertexIndex].y * instance.height);
    const vec2 base = vec2(instance.x, instance.y);
    gl_Position = mvpMatrix * vec4(base + vert, 1, 1);
}
