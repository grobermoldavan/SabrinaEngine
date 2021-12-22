#version 450

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inTint;

layout(location = 0) out vec4 outColor;

// One-dimensional cubic bezier curve
float bezier(float p0, float p1, float p2, float t)
{
    return p1 + (1 - t) * (1 - t) * (p0 - p1) + t * t * (p2 - p1);
}

void main()
{
    const float colX = bezier(0, 0.5, 0, inUv.x);
    const float colY = bezier(0, 0.5, 0, inUv.y);
    const float col = min(colY, colX);
    outColor = inTint * vec4(inUv.y * inUv.y * inUv.y, col, col, 1);
}
