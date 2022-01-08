#ifndef _SE_MATH_H_
#define _SE_MATH_H_

/*
    Sabrina Engine math library.

    Coordinate system is left handed.
    Positive X - right direction
    Positive Y - up direction
    Positive Z - forward direction
    Cross product is right handed (https://en.wikipedia.org/wiki/Cross_product)
*/

#include <math.h>

#define SE_PI 3.14159265358979323846
#define SE_EPSILON 0.0000001
#define SE_MAX_FLOAT 3.402823466e+38F

#define se_to_radians(degrees) (degrees * SE_PI / 180.0f)
#define se_to_degrees(radians) (radians * 180.0f / SE_PI)
#define se_is_equal_float(value1, value2) ((value1 > value2 ? value1 - value2 : value2 - value1) < SE_EPSILON)

#define se_clamp(val, min, max) (val < min ? min : (val > max ? max : val))
#define se_min(a, b) (a < b ? a : b)
#define se_max(a, b) (a > b ? a : b)

typedef struct SeFloat2
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        struct
        {
            float u;
            float v;
        };
        float elements[2];
    };
} SeFloat2;

float       se_f2_dot(SeFloat2 first, SeFloat2 second);
SeFloat2    se_f2_add(SeFloat2 first, SeFloat2 second);
SeFloat2    se_f2_sub(SeFloat2 first, SeFloat2 second);
SeFloat2    se_f2_mul(SeFloat2 vec, float value);
SeFloat2    se_f2_div(SeFloat2 vec, float value);
float       se_f2_len(SeFloat2 vec);
float       se_f2_len2(SeFloat2 vec);
SeFloat2    se_f2_normalized(SeFloat2 vec);

typedef struct SeFloat3
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };
        struct
        {
            float r;
            float g;
            float b;
        };
        float elements[3];
    };
} SeFloat3;

float       se_f3_dot(SeFloat3 first, SeFloat3 second);
SeFloat3    se_f3_add(SeFloat3 first, SeFloat3 second);
SeFloat3    se_f3_sub(SeFloat3 first, SeFloat3 second);
SeFloat3    se_f3_mul(SeFloat3 vec, float value);
SeFloat3    se_f3_div(SeFloat3 vec, float value);
float       se_f3_len(SeFloat3 vec);
float       se_f3_len2(SeFloat3 vec);
SeFloat3    se_f3_normalized(SeFloat3 vec);
SeFloat3    se_f3_cross(SeFloat3 a, SeFloat3 b);

typedef struct SeFloat4
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        struct
        {
            float r;
            float g;
            float b;
            float a;
        };
        float elements[4];
    };
} SeFloat4;

float       se_f4_dot(SeFloat4 first, SeFloat4 second);
SeFloat4    se_f4_add(SeFloat4 first, SeFloat4 second);
SeFloat4    se_f4_sub(SeFloat4 first, SeFloat4 second);
SeFloat4    se_f4_mul(SeFloat4 vec, float value);
SeFloat4    se_f4_div(SeFloat4 vec, float value);
float       se_f4_len(SeFloat4 vec);
float       se_f4_len2(SeFloat4 vec);
SeFloat4    se_f4_normalized(SeFloat4 vec);
SeFloat4    se_f4_cross(SeFloat4 a, SeFloat4 b);

typedef struct SeFloat4x4
{
    union
    {
        struct
        {
            float _00, _01, _02, _03;
            float _10, _11, _12, _13;
            float _20, _21, _22, _23;
            float _30, _31, _32, _33;
        };
        SeFloat4 rows[4];
        float elements[16];
        float m[4][4];
    };
} SeFloat4x4;

const SeFloat4x4 SE_F4X4_IDENTITY =
{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

SeFloat4x4  se_f4x4_mul_f4x4(SeFloat4x4 first, SeFloat4x4 second);
SeFloat4x4  se_f4x4_mul_f(SeFloat4x4 mat, float value);
SeFloat4    se_f4x4_mul_f4(SeFloat4x4 mat, SeFloat4 value);
SeFloat4x4  se_f4x4_transposed(SeFloat4x4 mat);
float       se_f4x4_det(SeFloat4x4 mat);
SeFloat4x4  se_f4x4_inverted(SeFloat4x4 mat);
SeFloat4x4  se_f4x4_translation(SeFloat3 position);
SeFloat4x4  se_f4x4_rotation(SeFloat3 eulerAngles);
SeFloat4x4  se_f4x4_scale(SeFloat3 scale);

//
// Quaternion conversions are taken from www.euclideanspace.com
//
// @TODO :  optimize conversions
//
typedef struct SeQuaternion
{
    union
    {
        struct
        {
            float real;
            SeFloat3 imaginary;
        };
        struct
        {
            float w, x, y, z;
        };
        float elements[4];
    };
} SeQuaternion;

const SeQuaternion SE_Q_IDENTITY =
{
    .real = 1.0f,
    .imaginary = { 0, 0, 0 }
};

typedef struct SeAxisAngle
{
    SeFloat3 axis;
    float angle;
} SeAxisAngle;

SeQuaternion    se_q_conjugate(SeQuaternion quat); // inverted
SeQuaternion    se_q_from_axis_angle(SeAxisAngle axisAngle);
SeAxisAngle     se_q_to_axis_angle(SeQuaternion quat);
SeQuaternion    se_q_from_euler_angles(SeFloat3 angles);
SeFloat3        se_q_to_euler_angles(SeQuaternion quat);
SeQuaternion    se_q_from_rotation_mat(SeFloat4x4 mat);
SeFloat4x4      se_q_to_rotation_mat(SeQuaternion quat);
SeQuaternion    se_q_from_mat(SeFloat4x4 mat);
SeQuaternion    se_q_add(SeQuaternion first, SeQuaternion second);
SeQuaternion    se_q_sub(SeQuaternion first, SeQuaternion second);
SeQuaternion    se_q_mul_q(SeQuaternion first, SeQuaternion second);
SeQuaternion    se_q_mul_f(SeQuaternion quat, float scalar);
SeQuaternion    se_q_div_q(SeQuaternion first, SeQuaternion second);
SeQuaternion    se_q_div_f(SeQuaternion quat, float scalar);
float           se_q_len(SeQuaternion quat);
float           se_q_len2(SeQuaternion quat);
SeQuaternion    se_q_normalized(SeQuaternion quat);

typedef struct SeTransform
{
    SeFloat4x4 matrix;
} SeTransform;

const SeTransform SE_T_IDENTITY =
{
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    },
};

SeFloat3    se_t_get_position(SeTransform* trf);
SeFloat3    se_t_get_rotation(SeTransform* trf);
SeFloat3    se_t_get_scale(SeTransform* trf);
SeFloat3    se_t_get_forward(SeTransform* trf);
SeFloat3    se_t_get_right(SeTransform* trf);
SeFloat3    se_t_get_up(SeTransform* trf);
SeFloat3    se_t_get_inverted_scale(SeTransform* trf);
void        se_t_set_position(SeTransform* trf, SeFloat3 position);
void        se_t_set_rotation(SeTransform* trf, SeFloat3 eulerAngles);
void        se_t_set_scale(SeTransform* trf, SeFloat3 scale);
void        se_t_look_at(SeTransform* trf, SeFloat3 from, SeFloat3 to, SeFloat3 up);

SeFloat4x4 se_perspective_projection(float fovDeg, float aspect, float near, float far);
SeFloat4x4 se_look_at(SeFloat3 from, SeFloat3 to, SeFloat3 up);

// =================================================================================================================
//
// Implementation
//
// =================================================================================================================

float       se_f2_dot(SeFloat2 first, SeFloat2 second)  { return first.x * second.x + first.y * second.y; }
SeFloat2    se_f2_add(SeFloat2 first, SeFloat2 second)  { return (SeFloat2){ first.x + second.x, first.y + second.y }; }
SeFloat2    se_f2_sub(SeFloat2 first, SeFloat2 second)  { return (SeFloat2){ first.x - second.x, first.y - second.y }; }
SeFloat2    se_f2_mul(SeFloat2 vec, float value)        { return (SeFloat2){ vec.x * value, vec.y * value }; }
SeFloat2    se_f2_div(SeFloat2 vec, float value)        { return (SeFloat2){ vec.x / value, vec.y / value }; }
float       se_f2_len(SeFloat2 vec)                     { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
float       se_f2_len2(SeFloat2 vec)                    { return vec.x * vec.x + vec.y * vec.y; }
SeFloat2    se_f2_normalized(SeFloat2 vec)              { float length = se_f2_len(vec); return (SeFloat2){ vec.x / length, vec.y / length }; }

float       se_f3_dot(SeFloat3 first, SeFloat3 second)  { return first.x * second.x + first.y * second.y + first.z * second.z; }
SeFloat3    se_f3_add(SeFloat3 first, SeFloat3 second)  { return (SeFloat3){ first.x + second.x, first.y + second.y, first.z + second.z }; }
SeFloat3    se_f3_sub(SeFloat3 first, SeFloat3 second)  { return (SeFloat3){ first.x - second.x, first.y - second.y, first.z - second.z }; }
SeFloat3    se_f3_mul(SeFloat3 vec, float value)        { return (SeFloat3){ vec.x * value, vec.y * value, vec.z * value }; }
SeFloat3    se_f3_div(SeFloat3 vec, float value)        { return (SeFloat3){ vec.x / value, vec.y / value, vec.z / value }; }
float       se_f3_len(SeFloat3 vec)                     { return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z); }
float       se_f3_len2(SeFloat3 vec)                    { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z; }
SeFloat3    se_f3_normalized(SeFloat3 vec)              { float length = se_f3_len(vec); return (SeFloat3){ vec.x / length, vec.y / length, vec.z / length }; }
SeFloat3    se_f3_cross(SeFloat3 a, SeFloat3 b)         { return (SeFloat3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

float       se_f4_dot(SeFloat4 first, SeFloat4 second)  { return first.x * second.x + first.y * second.y + first.z * second.z + first.w * second.w; }
SeFloat4    se_f4_add(SeFloat4 first, SeFloat4 second)  { return (SeFloat4){ first.x + second.x, first.y + second.y, first.z + second.z, first.w + second.w }; }
SeFloat4    se_f4_sub(SeFloat4 first, SeFloat4 second)  { return (SeFloat4){ first.x - second.x, first.y - second.y, first.z - second.z, first.w - second.w }; }
SeFloat4    se_f4_mul(SeFloat4 vec, float value)        { return (SeFloat4){ vec.x * value, vec.y * value, vec.z * value, vec.w * value }; }
SeFloat4    se_f4_div(SeFloat4 vec, float value)        { return (SeFloat4){ vec.x / value, vec.y / value, vec.z / value, vec.w / value }; }
float       se_f4_len(SeFloat4 vec)                     { return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }
float       se_f4_len2(SeFloat4 vec)                    { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w; }
SeFloat4    se_f4_normalized(SeFloat4 vec)              { float length = se_f4_len(vec); return (SeFloat4){ vec.x / length, vec.y / length, vec.z / length, vec.w / length }; }

SeFloat4x4 se_f4x4_mul_f4x4(SeFloat4x4 first, SeFloat4x4 second)
{
    return (SeFloat4x4)
    {
        (second.m[0][0] * first.m[0][0]) + (second.m[1][0] * first.m[0][1]) + (second.m[2][0] * first.m[0][2]) + (second.m[3][0] * first.m[0][3]),  // 0 0
        (second.m[0][1] * first.m[0][0]) + (second.m[1][1] * first.m[0][1]) + (second.m[2][1] * first.m[0][2]) + (second.m[3][1] * first.m[0][3]),  // 0 1
        (second.m[0][2] * first.m[0][0]) + (second.m[1][2] * first.m[0][1]) + (second.m[2][2] * first.m[0][2]) + (second.m[3][2] * first.m[0][3]),  // 0 2
        (second.m[0][3] * first.m[0][0]) + (second.m[1][3] * first.m[0][1]) + (second.m[2][3] * first.m[0][2]) + (second.m[3][3] * first.m[0][3]),  // 0 3
        (second.m[0][0] * first.m[1][0]) + (second.m[1][0] * first.m[1][1]) + (second.m[2][0] * first.m[1][2]) + (second.m[3][0] * first.m[1][3]),  // 1 0
        (second.m[0][1] * first.m[1][0]) + (second.m[1][1] * first.m[1][1]) + (second.m[2][1] * first.m[1][2]) + (second.m[3][1] * first.m[1][3]),  // 1 1
        (second.m[0][2] * first.m[1][0]) + (second.m[1][2] * first.m[1][1]) + (second.m[2][2] * first.m[1][2]) + (second.m[3][2] * first.m[1][3]),  // 1 2
        (second.m[0][3] * first.m[1][0]) + (second.m[1][3] * first.m[1][1]) + (second.m[2][3] * first.m[1][2]) + (second.m[3][3] * first.m[1][3]),  // 1 3
        (second.m[0][0] * first.m[2][0]) + (second.m[1][0] * first.m[2][1]) + (second.m[2][0] * first.m[2][2]) + (second.m[3][0] * first.m[2][3]),  // 2 0
        (second.m[0][1] * first.m[2][0]) + (second.m[1][1] * first.m[2][1]) + (second.m[2][1] * first.m[2][2]) + (second.m[3][1] * first.m[2][3]),  // 2 1
        (second.m[0][2] * first.m[2][0]) + (second.m[1][2] * first.m[2][1]) + (second.m[2][2] * first.m[2][2]) + (second.m[3][2] * first.m[2][3]),  // 2 2
        (second.m[0][3] * first.m[2][0]) + (second.m[1][3] * first.m[2][1]) + (second.m[2][3] * first.m[2][2]) + (second.m[3][3] * first.m[2][3]),  // 2 3
        (second.m[0][0] * first.m[3][0]) + (second.m[1][0] * first.m[3][1]) + (second.m[2][0] * first.m[3][2]) + (second.m[3][0] * first.m[3][3]),  // 3 0
        (second.m[0][1] * first.m[3][0]) + (second.m[1][1] * first.m[3][1]) + (second.m[2][1] * first.m[3][2]) + (second.m[3][1] * first.m[3][3]),  // 3 1
        (second.m[0][2] * first.m[3][0]) + (second.m[1][2] * first.m[3][1]) + (second.m[2][2] * first.m[3][2]) + (second.m[3][2] * first.m[3][3]),  // 3 2
        (second.m[0][3] * first.m[3][0]) + (second.m[1][3] * first.m[3][1]) + (second.m[2][3] * first.m[3][2]) + (second.m[3][3] * first.m[3][3])   // 3 3
    };
}

SeFloat4x4 se_f4x4_mul_f(SeFloat4x4 mat, float value)
{
    return (SeFloat4x4)
    {
        mat.m[0][0] * value, mat.m[0][1] * value, mat.m[0][2] * value, mat.m[0][3] * value,
        mat.m[1][0] * value, mat.m[1][1] * value, mat.m[1][2] * value, mat.m[1][3] * value,
        mat.m[2][0] * value, mat.m[2][1] * value, mat.m[2][2] * value, mat.m[2][3] * value,
        mat.m[3][0] * value, mat.m[3][1] * value, mat.m[3][2] * value, mat.m[3][3] * value
    };
}

SeFloat4 se_f4x4_mul_f4(SeFloat4x4 mat, SeFloat4 value)
{
    return (SeFloat4)
    {
        value.x * mat.m[0][0] + value.y * mat.m[0][1] + value.z * mat.m[0][2] + value.w * mat.m[0][3],
        value.x * mat.m[1][0] + value.y * mat.m[1][1] + value.z * mat.m[1][2] + value.w * mat.m[1][3],
        value.x * mat.m[2][0] + value.y * mat.m[2][1] + value.z * mat.m[2][2] + value.w * mat.m[2][3],
        value.x * mat.m[3][0] + value.y * mat.m[3][1] + value.z * mat.m[3][2] + value.w * mat.m[3][3]
    };
}

SeFloat4x4 se_f4x4_transposed(SeFloat4x4 mat)
{
    return (SeFloat4x4)
    {
        mat._00, mat._10, mat._20, mat._30,
        mat._01, mat._11, mat._21, mat._31,
        mat._02, mat._12, mat._22, mat._32,
        mat._03, mat._13, mat._23, mat._33
    };
}

float se_f4x4_det(SeFloat4x4 mat)
{
    // 2x2 sub-determinants
    const float det2_01_01 = mat.m[0][0] * mat.m[1][1] - mat.m[0][1] * mat.m[1][0];
    const float det2_01_02 = mat.m[0][0] * mat.m[1][2] - mat.m[0][2] * mat.m[1][0];
    const float det2_01_03 = mat.m[0][0] * mat.m[1][3] - mat.m[0][3] * mat.m[1][0];
    const float det2_01_12 = mat.m[0][1] * mat.m[1][2] - mat.m[0][2] * mat.m[1][1];
    const float det2_01_13 = mat.m[0][1] * mat.m[1][3] - mat.m[0][3] * mat.m[1][1];
    const float det2_01_23 = mat.m[0][2] * mat.m[1][3] - mat.m[0][3] * mat.m[1][2];
    // 3x3 sub-determinants
    const float det3_201_012 = mat.m[2][0] * det2_01_12 - mat.m[2][1] * det2_01_02 + mat.m[2][2] * det2_01_01;
    const float det3_201_013 = mat.m[2][0] * det2_01_13 - mat.m[2][1] * det2_01_03 + mat.m[2][3] * det2_01_01;
    const float det3_201_023 = mat.m[2][0] * det2_01_23 - mat.m[2][2] * det2_01_03 + mat.m[2][3] * det2_01_02;
    const float det3_201_123 = mat.m[2][1] * det2_01_23 - mat.m[2][2] * det2_01_13 + mat.m[2][3] * det2_01_12;
    return -1.0f * det3_201_123 * mat.m[3][0] + det3_201_023 * mat.m[3][1] - det3_201_013 * mat.m[3][2] + det3_201_012 * mat.m[3][3];
}

SeFloat4x4 se_f4x4_inverted(SeFloat4x4 mat)
{
    // https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    // https://github.com/willnode/N-Matrix-Programmer
    const float determinant = se_f4x4_det(mat);
    if (se_is_equal_float(determinant, 0.0f))
    {
        // @TODO : add method which returns bool here
        // Can't invert
        return (SeFloat4x4){0};
    }
    const float invDet = 1.0f / determinant;
    const float A2323 = mat.m[2][2] * mat.m[3][3] - mat.m[2][3] * mat.m[3][2];
    const float A1323 = mat.m[2][1] * mat.m[3][3] - mat.m[2][3] * mat.m[3][1];
    const float A1223 = mat.m[2][1] * mat.m[3][2] - mat.m[2][2] * mat.m[3][1];
    const float A0323 = mat.m[2][0] * mat.m[3][3] - mat.m[2][3] * mat.m[3][0];
    const float A0223 = mat.m[2][0] * mat.m[3][2] - mat.m[2][2] * mat.m[3][0];
    const float A0123 = mat.m[2][0] * mat.m[3][1] - mat.m[2][1] * mat.m[3][0];
    const float A2313 = mat.m[1][2] * mat.m[3][3] - mat.m[1][3] * mat.m[3][2];
    const float A1313 = mat.m[1][1] * mat.m[3][3] - mat.m[1][3] * mat.m[3][1];
    const float A1213 = mat.m[1][1] * mat.m[3][2] - mat.m[1][2] * mat.m[3][1];
    const float A2312 = mat.m[1][2] * mat.m[2][3] - mat.m[1][3] * mat.m[2][2];
    const float A1312 = mat.m[1][1] * mat.m[2][3] - mat.m[1][3] * mat.m[2][1];
    const float A1212 = mat.m[1][1] * mat.m[2][2] - mat.m[1][2] * mat.m[2][1];
    const float A0313 = mat.m[1][0] * mat.m[3][3] - mat.m[1][3] * mat.m[3][0];
    const float A0213 = mat.m[1][0] * mat.m[3][2] - mat.m[1][2] * mat.m[3][0];
    const float A0312 = mat.m[1][0] * mat.m[2][3] - mat.m[1][3] * mat.m[2][0];
    const float A0212 = mat.m[1][0] * mat.m[2][2] - mat.m[1][2] * mat.m[2][0];
    const float A0113 = mat.m[1][0] * mat.m[3][1] - mat.m[1][1] * mat.m[3][0];
    const float A0112 = mat.m[1][0] * mat.m[2][1] - mat.m[1][1] * mat.m[2][0];
    return (SeFloat4x4)
    {
        invDet          * (mat.m[1][1] * A2323 - mat.m[1][2] * A1323 + mat.m[1][3] * A1223),    // 0 0
        invDet * -1.0f  * (mat.m[0][1] * A2323 - mat.m[0][2] * A1323 + mat.m[0][3] * A1223),    // 0 1
        invDet          * (mat.m[0][1] * A2313 - mat.m[0][2] * A1313 + mat.m[0][3] * A1213),    // 0 2
        invDet * -1.0f  * (mat.m[0][1] * A2312 - mat.m[0][2] * A1312 + mat.m[0][3] * A1212),    // 0 3
        invDet * -1.0f  * (mat.m[1][0] * A2323 - mat.m[1][2] * A0323 + mat.m[1][3] * A0223),    // 1 0
        invDet          * (mat.m[0][0] * A2323 - mat.m[0][2] * A0323 + mat.m[0][3] * A0223),    // 1 1
        invDet * -1.0f  * (mat.m[0][0] * A2313 - mat.m[0][2] * A0313 + mat.m[0][3] * A0213),    // 1 2
        invDet          * (mat.m[0][0] * A2312 - mat.m[0][2] * A0312 + mat.m[0][3] * A0212),    // 1 3
        invDet          * (mat.m[1][0] * A1323 - mat.m[1][1] * A0323 + mat.m[1][3] * A0123),    // 2 0
        invDet * -1.0f  * (mat.m[0][0] * A1323 - mat.m[0][1] * A0323 + mat.m[0][3] * A0123),    // 2 1
        invDet          * (mat.m[0][0] * A1313 - mat.m[0][1] * A0313 + mat.m[0][3] * A0113),    // 2 2
        invDet * -1.0f  * (mat.m[0][0] * A1312 - mat.m[0][1] * A0312 + mat.m[0][3] * A0112),    // 2 3
        invDet * -1.0f  * (mat.m[1][0] * A1223 - mat.m[1][1] * A0223 + mat.m[1][2] * A0123),    // 3 0
        invDet          * (mat.m[0][0] * A1223 - mat.m[0][1] * A0223 + mat.m[0][2] * A0123),    // 3 1
        invDet * -1.0f  * (mat.m[0][0] * A1213 - mat.m[0][1] * A0213 + mat.m[0][2] * A0113),    // 3 2
        invDet          * (mat.m[0][0] * A1212 - mat.m[0][1] * A0212 + mat.m[0][2] * A0112)     // 3 3
    };
}

SeFloat4x4 se_f4x4_translation(SeFloat3 position)
{
    return (SeFloat4x4)
    {
        1, 0, 0, position.x,
        0, 1, 0, position.y,
        0, 0, 1, position.z,
        0, 0, 0, 1,
    };
}

SeFloat4x4 se_f4x4_rotation(SeFloat3 eulerAngles)
{
    const float cosPitch = (float)(cosf(se_to_radians(eulerAngles.x)));
    const float sinPitch = (float)(sinf(se_to_radians(eulerAngles.x)));
    const float cosYaw   = (float)(cosf(se_to_radians(eulerAngles.y)));
    const float sinYaw   = (float)(sinf(se_to_radians(eulerAngles.y)));
    const float cosRoll  = (float)(cosf(se_to_radians(eulerAngles.z)));
    const float sinRoll  = (float)(sinf(se_to_radians(eulerAngles.z)));
    const SeFloat4x4 pitch = (SeFloat4x4)
    {
        1, 0       , 0        , 0,
        0, cosPitch, -sinPitch, 0,
        0, sinPitch,  cosPitch, 0,
        0, 0       , 0        , 1
    };
    const SeFloat4x4 yaw = (SeFloat4x4)
    {
        cosYaw , 0, sinYaw, 0,
        0      , 1, 0     , 0,
        -sinYaw, 0, cosYaw, 0,
        0      , 0, 0     , 1
    };
    const SeFloat4x4 roll = (SeFloat4x4)
    {
        cosRoll, -sinRoll, 0, 0,
        sinRoll,  cosRoll, 0, 0,
        0      , 0       , 1, 0,
        0      , 0       , 0, 1
    };
    return se_f4x4_mul_f4x4(yaw, se_f4x4_mul_f4x4(roll, pitch));
}

SeFloat4x4 se_f4x4_scale(SeFloat3 scale)
{
    return (SeFloat4x4)
    {
        scale.x , 0         , 0         , 0,
        0       , scale.y   , 0         , 0,
        0       , 0         , scale.z   , 0,
        0       , 0         , 0         , 1
    };
}


SeQuaternion se_q_conjugate(SeQuaternion quat) // inverted
{
    return (SeQuaternion)
    {
        quat.real,
        se_f3_mul(quat.imaginary, -1.0f)
    };
}

SeQuaternion se_q_from_axis_angle(SeAxisAngle axisAngle)
{
    const float halfAngle = se_to_radians(axisAngle.angle) * 0.5f;
    const float halfAnglesin = sin(halfAngle);
    return (SeQuaternion)
    {
        .w = cos(halfAngle),
        .x = axisAngle.axis.x * halfAnglesin,
        .y = axisAngle.axis.y * halfAnglesin,
        .z = axisAngle.axis.z * halfAnglesin
    };
}

SeAxisAngle se_q_to_axis_angle(SeQuaternion quat)
{
    const float invSqrt = 1.0f / sqrt(1.0f - quat.w * quat.w);
    return (SeAxisAngle)
    {
        quat.x * invSqrt,
        quat.y * invSqrt,
        quat.z * invSqrt,
        se_to_degrees(2.0f * acos(quat.w))
    };
}

SeQuaternion se_q_from_euler_angles(SeFloat3 angles)
{
    const float halfY = se_to_radians(angles.y) * 0.5f;
    const float halfZ = se_to_radians(angles.z) * 0.5f;
    const float halfX = se_to_radians(angles.x) * 0.5f;
    const float c1 = cos(halfY);
    const float s1 = sin(halfY);
    const float c2 = cos(halfZ);
    const float s2 = sin(halfZ);
    const float c3 = cos(halfX);
    const float s3 = sin(halfX);
    const float c1c2 = c1 * c2;
    const float s1s2 = s1 * s2;
    return (SeQuaternion)
    {
        .w = c1c2 * c3 - s1s2 * s3,
        .x = c1c2 * s3 + s1s2 * c3,
        .y = s1 * c2 * c3 + c1 * s2 * s3,
        .z = c1 * s2 * c3 - s1 * c2 * s3
    };
}

SeFloat3 se_q_to_euler_angles(SeQuaternion quat)
{
    const float test = quat.x * quat.y + quat.z * quat.w;
    if (se_is_equal_float(test, 0.5f) || test > 0.5f)
    {
        // singularity at north pole
        float heading = 2.0f * atan2(quat.x, quat.w);
        float attitude = SE_PI * 0.5f;
        float bank = 0.0f;
        return (SeFloat3)
        {
            se_to_degrees(bank),
            se_to_degrees(heading),
            se_to_degrees(attitude)
        };
    }
    if (test < -0.499)
    {
        // singularity at south pole
        float heading = -2.0f * atan2(quat.x, quat.w);
        float attitude = -SE_PI * 0.5f;
        float bank = 0.0f;
        return (SeFloat3)
        {
            se_to_degrees(bank),
            se_to_degrees(heading),
            se_to_degrees(attitude)
        };
    }
    float sqx = quat.x * quat.x;
    float sqy = quat.y * quat.y;
    float sqz = quat.z * quat.z;
    float heading = atan2(2.0f * quat.y * quat.w - 2.0f * quat.x * quat.z, 1.0f - 2.0f * sqy - 2.0f * sqz);
    float attitude = asin(2.0f * test);
    float bank = atan2(2.0f * quat.x * quat.w - 2.0f * quat.y * quat.z, 1.0f - 2.0f * sqx - 2.0f * sqz);
    return (SeFloat3)
    {
        se_to_degrees(bank),
        se_to_degrees(heading),
        se_to_degrees(attitude)
    };
}

SeQuaternion se_q_from_rotation_mat(SeFloat4x4 mat)
{
    const float tr = mat._00 + mat._11 + mat._22;
    if (!se_is_equal_float(tr, 0.0f) && tr > 0.0f)
    {
        float S = sqrt(tr + 1.0f) * 2.0f;
        return (SeQuaternion)
        {
            .w = 0.25f * S,
            .x = (mat._21 - mat._12) / S,
            .y = (mat._02 - mat._20) / S,
            .z = (mat._10 - mat._01) / S
        };
    }
    else if ((mat._00 > mat._11) && (mat._00 > mat._22))
    {
        float S = sqrt(1.0f + mat._00 - mat._11 - mat._22) * 2.0f;
        return (SeQuaternion)
        {
            .w = (mat._21 - mat._12) / S,
            .x = 0.25f * S,
            .y = (mat._01 + mat._10) / S,
            .z = (mat._02 + mat._20) / S
        };
    }
    else if (mat._11 > mat._22)
    {
        float S = sqrt(1.0f + mat._11 - mat._00 - mat._22) * 2.0f;
        return (SeQuaternion)
        {
            .w = (mat._02 - mat._20) / S,
            .x = (mat._01 + mat._10) / S,
            .y = 0.25f * S,
            .z = (mat._12 + mat._21) / S
        };
    }
    else
    {
        float S = sqrt(1.0f + mat._22 - mat._00 - mat._11) * 2.0f;
        return (SeQuaternion)
        {
            .w = (mat._10 - mat._01) / S,
            .x = (mat._02 + mat._20) / S,
            .y = (mat._12 + mat._21) / S,
            .z = 0.25f * S
        };
    }
}

SeFloat4x4 se_q_to_rotation_mat(SeQuaternion quat)
{
    const float xx = quat.x * quat.x;
    const float xy = quat.x * quat.y;
    const float xz = quat.x * quat.z;
    const float xw = quat.x * quat.w;
    const float yy = quat.y * quat.y;
    const float yz = quat.y * quat.z;
    const float yw = quat.y * quat.w;
    const float zz = quat.z * quat.z;
    const float zw = quat.z * quat.w;
    const float m00 = 1.0f - 2.0f * (yy + zz);
    const float m01 =        2.0f * (xy - zw);
    const float m02 =        2.0f * (xz + yw);
    const float m10 =        2.0f * (xy + zw);
    const float m11 = 1.0f - 2.0f * (xx + zz);
    const float m12 =        2.0f * (yz - xw);
    const float m20 =        2.0f * (xz - yw);
    const float m21 =        2.0f * (yz + xw);
    const float m22 = 1.0f - 2.0f * (xx + yy);
    return (SeFloat4x4)
    {
        m00, m01, m02, 0,
        m10, m11, m12, 0,
        m20, m21, m22, 0,
        0  , 0  , 0  , 1
    };
}

SeQuaternion se_q_from_mat(SeFloat4x4 mat)
{
    SeFloat3 inversedScale = (SeFloat3)
    {
        se_f3_len((SeFloat3){ mat._00, mat._01, mat._02 }),
        se_f3_len((SeFloat3){ mat._10, mat._11, mat._12 }),
        se_f3_len((SeFloat3){ mat._20, mat._21, mat._22 })
    };
    inversedScale.y = se_is_equal_float(inversedScale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.y;
    inversedScale.x = se_is_equal_float(inversedScale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.x;
    inversedScale.z = se_is_equal_float(inversedScale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.z;
    SeFloat4x4 rotationMatrix = (SeFloat4x4)
    {
        mat._00 * inversedScale.x, mat._01 * inversedScale.y, mat._02 * inversedScale.z, 0.0f,
        mat._10 * inversedScale.x, mat._11 * inversedScale.y, mat._12 * inversedScale.z, 0.0f,
        mat._20 * inversedScale.x, mat._21 * inversedScale.y, mat._22 * inversedScale.z, 0.0f,
        0.0f                     , 0.0f                     , 0.0f                     , 1.0f
    };
    return se_q_from_rotation_mat(rotationMatrix);
}

SeQuaternion se_q_add(SeQuaternion first, SeQuaternion second)
{
    return (SeQuaternion)
    {
        first.real + second.real,
        se_f3_add(first.imaginary, second.imaginary)
    };
}

SeQuaternion se_q_sub(SeQuaternion first, SeQuaternion second)
{
    return (SeQuaternion)
    {
        first.real + second.real,
        se_f3_sub(first.imaginary, second.imaginary)
    };
}

SeQuaternion se_q_mul_q(SeQuaternion first, SeQuaternion second)
{
    // https://www.cprogramming.com/tutorial/3d/quaternions.html
    return (SeQuaternion)
    {
        first.real * second.real - first.imaginary.x * second.imaginary.x - first.imaginary.y * second.imaginary.y - first.imaginary.z * second.imaginary.z,
        {
            first.real * second.imaginary.x + first.imaginary.x * second.real         + first.imaginary.y * second.imaginary.z   - first.imaginary.z * second.imaginary.y,
            first.real * second.imaginary.y - first.imaginary.x * second.imaginary.z  + first.imaginary.y * second.real          + first.imaginary.z * second.imaginary.x,
            first.real * second.imaginary.z + first.imaginary.x * second.imaginary.y  - first.imaginary.y * second.imaginary.x   + first.imaginary.z * second.real
        }
    };
}

SeQuaternion se_q_mul_f(SeQuaternion quat, float scalar)
{
    return (SeQuaternion)
    {
        quat.real * scalar,
        se_f3_mul(quat.imaginary, scalar)
    };
}

SeQuaternion se_q_div_q(SeQuaternion first, SeQuaternion second)
{
    return se_q_mul_q(first, se_q_conjugate(second));
}

SeQuaternion se_q_div_f(SeQuaternion quat, float scalar)
{
    return (SeQuaternion)
    {
        quat.real / scalar,
        se_f3_div(quat.imaginary, scalar)
    };
}

float se_q_len(SeQuaternion quat)
{
    return sqrt(quat.real * quat.real + se_f3_len2(quat.imaginary));
}

float se_q_len2(SeQuaternion quat)
{
    return quat.real * quat.real + se_f3_len2(quat.imaginary);
}

SeQuaternion se_q_normalized(SeQuaternion quat)
{
    const float length = se_q_len(quat);
    return (SeQuaternion)
    {
        quat.real / length,
        se_f3_div(quat.imaginary, length)
    };
}

SeFloat3 se_t_get_position(SeTransform* trf)
{
    return (SeFloat3){ trf->matrix._03, trf->matrix._13, trf->matrix._23 };
}

SeFloat3 se_t_get_rotation(SeTransform* trf)
{
    return se_q_to_euler_angles(se_q_from_mat(trf->matrix));
}

SeFloat3 se_t_get_scale(SeTransform* trf)
{
    return (SeFloat3)
    {
        se_f3_len((SeFloat3){ trf->matrix._00, trf->matrix._01, trf->matrix._02 }),
        se_f3_len((SeFloat3){ trf->matrix._10, trf->matrix._11, trf->matrix._12 }),
        se_f3_len((SeFloat3){ trf->matrix._20, trf->matrix._21, trf->matrix._22 })
    };
}

SeFloat3 se_t_get_forward(SeTransform* trf)
{
    return se_f3_normalized((SeFloat3){ trf->matrix.m[0][2], trf->matrix.m[1][2], trf->matrix.m[2][2] });
}

SeFloat3 se_t_get_right(SeTransform* trf)
{
    return se_f3_normalized((SeFloat3){ trf->matrix.m[0][0], trf->matrix.m[1][0], trf->matrix.m[2][0] });
}

SeFloat3 se_t_get_up(SeTransform* trf)
{
    return se_f3_normalized((SeFloat3){ trf->matrix.m[0][1], trf->matrix.m[1][1], trf->matrix.m[2][1] });
}

SeFloat3 se_t_get_inverted_scale(SeTransform* trf)
{
    SeFloat3 scale = se_t_get_scale(trf);
    return (SeFloat3)
    {
        se_is_equal_float(scale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.x,
        se_is_equal_float(scale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.y,
        se_is_equal_float(scale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.z
    };
}

void se_t_set_position(SeTransform* trf, SeFloat3 position)
{
    trf->matrix._03 = position.x;
    trf->matrix._13 = position.y;
    trf->matrix._23 = position.z;
}

void se_t_set_rotation(SeTransform* trf, SeFloat3 eulerAngles)
{
    SeFloat4x4 tr = se_f4x4_translation(se_t_get_position(trf));
    SeFloat4x4 rt = se_f4x4_rotation(eulerAngles);
    SeFloat4x4 sc = se_f4x4_scale(se_t_get_scale(trf));
    trf->matrix = se_f4x4_mul_f4x4(tr, se_f4x4_mul_f4x4(rt, sc));
}

void se_t_set_scale(SeTransform* trf, SeFloat3 scale)
{
    SeFloat4x4 tr = se_f4x4_translation(se_t_get_position(trf));
    SeFloat4x4 rt = se_f4x4_rotation(se_t_get_rotation(trf));
    SeFloat4x4 sc = se_f4x4_scale(scale);
    trf->matrix = se_f4x4_mul_f4x4(tr, se_f4x4_mul_f4x4(rt, sc));
}

void se_t_look_at(SeTransform* trf, SeFloat3 from, SeFloat3 to, SeFloat3 up)
{
    SeFloat4x4 tr = se_f4x4_translation(se_t_get_position(trf));
    SeFloat4x4 rt = se_look_at(from, to, up);
    SeFloat4x4 sc = se_f4x4_scale(se_t_get_scale(trf));
    trf->matrix = se_f4x4_mul_f4x4(tr, se_f4x4_mul_f4x4(rt, sc));
}

SeFloat4x4 se_look_at(SeFloat3 from, SeFloat3 to, SeFloat3 up)
{
    SeFloat3 forward        = se_f3_normalized(se_f3_sub(to, from));
    SeFloat3 right          = se_f3_normalized(se_f3_cross(forward, up));
    SeFloat3 correctedUp    = se_f3_normalized(se_f3_cross(right, forward));
    SeFloat4x4 rotation     = SE_F4X4_IDENTITY;
    rotation.m[0][0] = right.x;
    rotation.m[0][1] = correctedUp.x;
    rotation.m[0][2] = forward.x;
    rotation.m[1][0] = right.y;
    rotation.m[1][1] = correctedUp.y;
    rotation.m[1][2] = forward.y;
    rotation.m[2][0] = right.z;
    rotation.m[2][1] = correctedUp.z;
    rotation.m[2][2] = forward.z;
    return rotation;
}

#endif
