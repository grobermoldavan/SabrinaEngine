#version 450

layout(set = 2, binding = 0) uniform sampler2D grassTexture;
layout(set = 2, binding = 1) uniform sampler2D rockTexture;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;

void main()
{
    float textureScale = 10.0;
    vec2 yUV = position.xz / textureScale;
	vec2 xUV = position.zy / textureScale;
	vec2 zUV = position.xy / textureScale;

    vec4 yDiff = texture(grassTexture, yUV);
	vec4 xDiff = texture(rockTexture, xUV);
	vec4 zDiff = texture(rockTexture, zUV);

    float blendSharpness = 5;
    vec3 normalAbs = vec3(abs(normal.x), abs(normal.y), abs(normal.z));
    vec3 blendWeights = vec3(pow(normalAbs.x, blendSharpness), pow(normalAbs.y, blendSharpness), pow(normalAbs.z, blendSharpness));
	blendWeights = blendWeights / (blendWeights.x + blendWeights.y + blendWeights.z);
	vec4 diffuseColor = xDiff * blendWeights.x + yDiff * blendWeights.y + zDiff * blendWeights.z;

    vec3 lightDir = normalize(vec3(1, -1, 1));
    vec4 ambient = vec4(0.05, 0.05, 0.2, 1);
    outColor = diffuseColor * (clamp(dot(normal.xyz, lightDir * -1), 0, 1) + ambient);
}
