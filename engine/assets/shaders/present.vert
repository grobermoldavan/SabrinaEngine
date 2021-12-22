#version 450

layout (location = 0) out vec2 outUv;

//vec2 positions[3] = vec2[](
//    vec2(-1, -1),
//    vec2( 3, -1),
//    vec2(-1,  3)
//);

// 0
// [0, 0]
// [-1, -1, 0, 1]

// 1
// [2, 0]
// [3, -1, 0, 1]

// 2
// [0, 2]
// [-1, 3, 0, 1]

void main()
{
    outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUv * 2.0f + -1.0f, 0.0f, 1.0f);
    //gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
