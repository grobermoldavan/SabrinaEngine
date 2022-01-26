#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 normal;

void main()
{
    vec3 lightDir = normalize(vec3(1, -1, 1));
    vec4 ambient = vec4(0.05, 0.05, 0.2, 1);
    outColor = vec4(1) * clamp(dot(normal.xyz, lightDir * -1), 0, 1) + ambient;
}
