
#include <math.h>

#include "se_math.h"

float           se_f2_dot(struct SeFloat2 first, struct SeFloat2 second)    { return first.x * second.x + first.y * second.y; }
struct SeFloat2 se_f2_add(struct SeFloat2 first, struct SeFloat2 second)    { return (struct SeFloat2){ first.x + second.x, first.y + second.y }; }
struct SeFloat2 se_f2_sub(struct SeFloat2 first, struct SeFloat2 second)    { return (struct SeFloat2){ first.x - second.x, first.y - second.y }; }
struct SeFloat2 se_f2_mul(struct SeFloat2 vec, float value)                 { return (struct SeFloat2){ vec.x * value, vec.y * value }; }
struct SeFloat2 se_f2_div(struct SeFloat2 vec, float value)                 { return (struct SeFloat2){ vec.x / value, vec.y / value }; }
float           se_f2_len(struct SeFloat2 vec)                              { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
float           se_f2_len2(struct SeFloat2 vec)                             { return vec.x * vec.x + vec.y * vec.y; }
struct SeFloat2 se_f2_normalized(struct SeFloat2 vec)                       { float length = se_f2_len(vec); return (struct SeFloat2){ vec.x / length, vec.y / length }; }

float           se_f3_dot(struct SeFloat3 first, struct SeFloat3 second)    { return first.x * second.x + first.y * second.y + first.z * second.z; }
struct SeFloat3 se_f3_add(struct SeFloat3 first, struct SeFloat3 second)    { return (struct SeFloat3){ first.x + second.x, first.y + second.y, first.z + second.z }; }
struct SeFloat3 se_f3_sub(struct SeFloat3 first, struct SeFloat3 second)    { return (struct SeFloat3){ first.x - second.x, first.y - second.y, first.z - second.z }; }
struct SeFloat3 se_f3_mul(struct SeFloat3 vec, float value)                 { return (struct SeFloat3){ vec.x * value, vec.y * value, vec.z * value }; }
struct SeFloat3 se_f3_div(struct SeFloat3 vec, float value)                 { return (struct SeFloat3){ vec.x / value, vec.y / value, vec.z / value }; }
float           se_f3_len(struct SeFloat3 vec)                              { return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z); }
float           se_f3_len2(struct SeFloat3 vec)                             { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z; }
struct SeFloat3 se_f3_normalized(struct SeFloat3 vec)                       { float length = se_f3_len(vec); return (struct SeFloat3){ vec.x / length, vec.y / length, vec.z / length }; }
struct SeFloat3 se_f3_cross(struct SeFloat3 a, struct SeFloat3 b)           { return (struct SeFloat3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

float           se_f4_dot(struct SeFloat4 first, struct SeFloat4 second)    { return first.x * second.x + first.y * second.y + first.z * second.z + first.w * second.w; }
struct SeFloat4 se_f4_add(struct SeFloat4 first, struct SeFloat4 second)    { return (struct SeFloat4){ first.x + second.x, first.y + second.y, first.z + second.z, first.w + second.w }; }
struct SeFloat4 se_f4_sub(struct SeFloat4 first, struct SeFloat4 second)    { return (struct SeFloat4){ first.x - second.x, first.y - second.y, first.z - second.z, first.w - second.w }; }
struct SeFloat4 se_f4_mul(struct SeFloat4 vec, float value)                 { return (struct SeFloat4){ vec.x * value, vec.y * value, vec.z * value, vec.w * value }; }
struct SeFloat4 se_f4_div(struct SeFloat4 vec, float value)                 { return (struct SeFloat4){ vec.x / value, vec.y / value, vec.z / value, vec.w / value }; }
float           se_f4_len(struct SeFloat4 vec)                              { return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }
float           se_f4_len2(struct SeFloat4 vec)                             { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w; }
struct SeFloat4 se_f4_normalized(struct SeFloat4 vec)                       { float length = se_f4_len(vec); return (struct SeFloat4){ vec.x / length, vec.y / length, vec.z / length, vec.w / length }; }
struct SeFloat4 se_f4_cross(struct SeFloat4 a, struct SeFloat4 b)           { return (struct SeFloat4){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

struct SeFloat4x4 se_f4x4_mul_f4x4(struct SeFloat4x4 first, struct SeFloat4x4 second)
{
    return (struct SeFloat4x4)
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

struct SeFloat4x4 se_f4x4_mul_f(struct SeFloat4x4 mat, float value)
{
    return (struct SeFloat4x4)
    {
        mat.m[0][0] * value, mat.m[0][1] * value, mat.m[0][2] * value, mat.m[0][3] * value,
        mat.m[1][0] * value, mat.m[1][1] * value, mat.m[1][2] * value, mat.m[1][3] * value,
        mat.m[2][0] * value, mat.m[2][1] * value, mat.m[2][2] * value, mat.m[2][3] * value,
        mat.m[3][0] * value, mat.m[3][1] * value, mat.m[3][2] * value, mat.m[3][3] * value
    };
}

struct SeFloat4 se_f4x4_mul_f4(struct SeFloat4x4 mat, struct SeFloat4 value)
{
    return (struct SeFloat4)
    {
        value.x * mat.m[0][0] + value.y * mat.m[0][1] + value.z * mat.m[0][2] + value.w * mat.m[0][3],
        value.x * mat.m[1][0] + value.y * mat.m[1][1] + value.z * mat.m[1][2] + value.w * mat.m[1][3],
        value.x * mat.m[2][0] + value.y * mat.m[2][1] + value.z * mat.m[2][2] + value.w * mat.m[2][3],
        value.x * mat.m[3][0] + value.y * mat.m[3][1] + value.z * mat.m[3][2] + value.w * mat.m[3][3]
    };
}

struct SeFloat4x4 se_f4x4_transposed(struct SeFloat4x4 mat)
{
    return (struct SeFloat4x4)
    {
        mat._00, mat._10, mat._20, mat._30,
        mat._01, mat._11, mat._21, mat._31,
        mat._02, mat._12, mat._22, mat._32,
        mat._03, mat._13, mat._23, mat._33
    };
}

float se_f4x4_det(struct SeFloat4x4 mat)
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

struct SeFloat4x4 se_f4x4_inverted(struct SeFloat4x4 mat)
{
    // https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    // https://github.com/willnode/N-Matrix-Programmer
    const float determinant = se_f4x4_det(mat);
    if (se_is_equal_float(determinant, 0.0f))
    {
        // @TODO : add method which returns bool here
        // Can't invert
        return (struct SeFloat4x4){0};
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
    return (struct SeFloat4x4)
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

struct SeFloat4x4 se_f4x4_translation(struct SeFloat3 position)
{
    return (struct SeFloat4x4)
    {
        1, 0, 0, position.x,
        0, 1, 0, position.y,
        0, 0, 1, position.z,
        0, 0, 0, 1,
    };
}

struct SeFloat4x4 se_f4x4_rotation(struct SeFloat3 eulerAngles)
{
    const float cosPitch = (float)(cosf(se_to_radians(eulerAngles.x)));
    const float sinPitch = (float)(sinf(se_to_radians(eulerAngles.x)));
    const float cosYaw   = (float)(cosf(se_to_radians(eulerAngles.y)));
    const float sinYaw   = (float)(sinf(se_to_radians(eulerAngles.y)));
    const float cosRoll  = (float)(cosf(se_to_radians(eulerAngles.z)));
    const float sinRoll  = (float)(sinf(se_to_radians(eulerAngles.z)));
    const struct SeFloat4x4 pitch = (struct SeFloat4x4)
    {
        1, 0       , 0        , 0,
        0, cosPitch, -sinPitch, 0,
        0, sinPitch,  cosPitch, 0,
        0, 0       , 0        , 1
    };
    const struct SeFloat4x4 yaw = (struct SeFloat4x4)
    {
        cosYaw , 0, sinYaw, 0,
        0      , 1, 0     , 0,
        -sinYaw, 0, cosYaw, 0,
        0      , 0, 0     , 1
    };
    const struct SeFloat4x4 roll = (struct SeFloat4x4)
    {
        cosRoll, -sinRoll, 0, 0,
        sinRoll,  cosRoll, 0, 0,
        0      , 0       , 1, 0,
        0      , 0       , 0, 1
    };
    return se_f4x4_mul_f4x4(yaw, se_f4x4_mul_f4x4(roll, pitch));
}

struct SeFloat4x4 se_f4x4_scale(struct SeFloat3 scale)
{
    return (struct SeFloat4x4)
    {
        scale.x , 0         , 0         , 0,
        0       , scale.y   , 0         , 0,
        0       , 0         , scale.z   , 0,
        0       , 0         , 0         , 1
    };
}


struct SeQuaternion se_q_conjugate(struct SeQuaternion quat) // inverted
{
    return (struct SeQuaternion)
    {
        quat.real,
        se_f3_mul(quat.imaginary, -1.0f)
    };
}

struct SeQuaternion se_q_from_axis_angle(struct SeAxisAngle axisAngle)
{
    const float halfAngle = se_to_radians(axisAngle.angle) * 0.5f;
    const float halfAnglesin = sin(halfAngle);
    return (struct SeQuaternion)
    {
        .w = cos(halfAngle),
        .x = axisAngle.axis.x * halfAnglesin,
        .y = axisAngle.axis.y * halfAnglesin,
        .z = axisAngle.axis.z * halfAnglesin
    };
}

struct SeAxisAngle se_q_to_axis_angle(struct SeQuaternion quat)
{
    const float invSqrt = 1.0f / sqrt(1.0f - quat.w * quat.w);
    return (struct SeAxisAngle)
    {
        quat.x * invSqrt,
        quat.y * invSqrt,
        quat.z * invSqrt,
        se_to_degrees(2.0f * acos(quat.w))
    };
}

struct SeQuaternion se_q_from_euler_angles(struct SeFloat3 angles)
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
    return (struct SeQuaternion)
    {
        .w = c1c2 * c3 - s1s2 * s3,
        .x = c1c2 * s3 + s1s2 * c3,
        .y = s1 * c2 * c3 + c1 * s2 * s3,
        .z = c1 * s2 * c3 - s1 * c2 * s3
    };
}

struct SeFloat3 se_q_to_euler_angles(struct SeQuaternion quat)
{
    const float test = quat.x * quat.y + quat.z * quat.w;
    if (se_is_equal_float(test, 0.5f) || test > 0.5f)
    {
        // singularity at north pole
        float heading = 2.0f * atan2(quat.x, quat.w);
        float attitude = SE_PI * 0.5f;
        float bank = 0.0f;
        return (struct SeFloat3)
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
        return (struct SeFloat3)
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
    return (struct SeFloat3)
    {
        se_to_degrees(bank),
        se_to_degrees(heading),
        se_to_degrees(attitude)
    };
}

struct SeQuaternion se_q_from_rotation_mat(struct SeFloat4x4 mat)
{
    const float tr = mat._00 + mat._11 + mat._22;
    if (!se_is_equal_float(tr, 0.0f) && tr > 0.0f)
    { 
        float S = sqrt(tr + 1.0f) * 2.0f;
        return (struct SeQuaternion)
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
        return (struct SeQuaternion)
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
        return (struct SeQuaternion)
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
        return (struct SeQuaternion)
        {
            .w = (mat._10 - mat._01) / S,
            .x = (mat._02 + mat._20) / S,
            .y = (mat._12 + mat._21) / S,
            .z = 0.25f * S
        };
    }
}

struct SeFloat4x4 se_q_to_rotation_mat(struct SeQuaternion quat)
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
    return (struct SeFloat4x4)
    {
        m00, m01, m02, 0,
        m10, m11, m12, 0,
        m20, m21, m22, 0,
        0  , 0  , 0  , 1
    };
}

struct SeQuaternion se_q_from_mat(struct SeFloat4x4 mat)
{
    struct SeFloat3 inversedScale = (struct SeFloat3)
    {
        se_f3_len((struct SeFloat3){ mat._00, mat._01, mat._02 }),
        se_f3_len((struct SeFloat3){ mat._10, mat._11, mat._12 }),
        se_f3_len((struct SeFloat3){ mat._20, mat._21, mat._22 })
    };
    inversedScale.y = se_is_equal_float(inversedScale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.y;
    inversedScale.x = se_is_equal_float(inversedScale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.x;
    inversedScale.z = se_is_equal_float(inversedScale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.z;
    struct SeFloat4x4 rotationMatrix = (struct SeFloat4x4)
    {
        mat._00 * inversedScale.x, mat._01 * inversedScale.y, mat._02 * inversedScale.z, 0.0f,
        mat._10 * inversedScale.x, mat._11 * inversedScale.y, mat._12 * inversedScale.z, 0.0f,
        mat._20 * inversedScale.x, mat._21 * inversedScale.y, mat._22 * inversedScale.z, 0.0f,
        0.0f                     , 0.0f                     , 0.0f                     , 1.0f
    };
    return se_q_from_rotation_mat(rotationMatrix);
}

struct SeQuaternion se_q_add(struct SeQuaternion first, struct SeQuaternion second)
{
    return (struct SeQuaternion)
    {
        first.real + second.real,
        se_f3_add(first.imaginary, second.imaginary)
    };
}

struct SeQuaternion se_q_sub(struct SeQuaternion first, struct SeQuaternion second)
{
    return (struct SeQuaternion)
    {
        first.real + second.real,
        se_f3_sub(first.imaginary, second.imaginary)
    };
}

struct SeQuaternion se_q_mul_q(struct SeQuaternion first, struct SeQuaternion second)
{
    // https://www.cprogramming.com/tutorial/3d/quaternions.html
    return (struct SeQuaternion)
    {
        first.real * second.real - first.imaginary.x * second.imaginary.x - first.imaginary.y * second.imaginary.y - first.imaginary.z * second.imaginary.z,
        {
            first.real * second.imaginary.x + first.imaginary.x * second.real         + first.imaginary.y * second.imaginary.z   - first.imaginary.z * second.imaginary.y,
            first.real * second.imaginary.y - first.imaginary.x * second.imaginary.z  + first.imaginary.y * second.real          + first.imaginary.z * second.imaginary.x,
            first.real * second.imaginary.z + first.imaginary.x * second.imaginary.y  - first.imaginary.y * second.imaginary.x   + first.imaginary.z * second.real
        }
    };
}

struct SeQuaternion se_q_mul_f(struct SeQuaternion quat, float scalar)
{
    return (struct SeQuaternion)
    {
        quat.real * scalar,
        se_f3_mul(quat.imaginary, scalar)
    };
}

struct SeQuaternion se_q_div_q(struct SeQuaternion first, struct SeQuaternion second)
{
    return se_q_mul_q(first, se_q_conjugate(second));
}

struct SeQuaternion se_q_div_f(struct SeQuaternion quat, float scalar)
{
    return (struct SeQuaternion)
    {
        quat.real / scalar,
        se_f3_div(quat.imaginary, scalar)
    };
}

float se_q_len(struct SeQuaternion quat)
{
    return sqrt(quat.real * quat.real + se_f3_len2(quat.imaginary));
}

float se_q_len2(struct SeQuaternion quat)
{
    return quat.real * quat.real + se_f3_len2(quat.imaginary);
}

struct SeQuaternion se_q_normalized(struct SeQuaternion quat)
{
    const float length = se_q_len(quat);
    return (struct SeQuaternion)
    {
        quat.real / length,
        se_f3_div(quat.imaginary, length)
    };
}

struct SeFloat3 se_t_get_position(struct SeTransform trf)
{
    return (struct SeFloat3){ trf.matrix._03, trf.matrix._13, trf.matrix._23 };
}

struct SeFloat3 se_t_get_rotation(struct SeTransform trf)
{
    return se_q_to_euler_angles(se_q_from_mat(trf.matrix));
}

struct SeFloat3 se_t_get_scale(struct SeTransform trf)
{
    return (struct SeFloat3)
    {
        se_f3_len((struct SeFloat3){ trf.matrix._00, trf.matrix._01, trf.matrix._02 }),
        se_f3_len((struct SeFloat3){ trf.matrix._10, trf.matrix._11, trf.matrix._12 }),
        se_f3_len((struct SeFloat3){ trf.matrix._20, trf.matrix._21, trf.matrix._22 })
    };
}

struct SeFloat3 se_t_get_forward(struct SeTransform trf)
{
    return se_f3_normalized((struct SeFloat3){ trf.matrix.m[0][2], trf.matrix.m[1][2], trf.matrix.m[2][2] });
}

struct SeFloat3 se_t_get_right(struct SeTransform trf)
{
    return se_f3_normalized((struct SeFloat3){ trf.matrix.m[0][0], trf.matrix.m[1][0], trf.matrix.m[2][0] });
}

struct SeFloat3 se_t_get_up(struct SeTransform trf)
{
    return se_f3_normalized((struct SeFloat3){ trf.matrix.m[0][1], trf.matrix.m[1][1], trf.matrix.m[2][1] });
}

struct SeFloat3 se_t_get_inverted_scale(struct SeTransform trf)
{
    struct SeFloat3 scale = se_t_get_scale(trf);
    return (struct SeFloat3)
    {
        se_is_equal_float(scale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.x,
        se_is_equal_float(scale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.y,
        se_is_equal_float(scale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.z
    };
}

void se_t_set_position(struct SeTransform* trf, struct SeFloat3 position)
{
    trf->matrix._03 = position.x;
    trf->matrix._13 = position.y;
    trf->matrix._23 = position.z;
}

void se_t_set_rotation(struct SeTransform* trf, struct SeFloat3 eulerAngles)
{
    struct SeFloat4x4 tr = se_f4x4_translation(se_t_get_position(*trf));
    struct SeFloat4x4 rt = se_f4x4_rotation(eulerAngles); 
    struct SeFloat4x4 sc = se_f4x4_scale(se_t_get_scale(*trf));
    trf->matrix = se_f4x4_mul_f4x4(tr, se_f4x4_mul_f4x4(rt, sc));
}

void se_t_set_scale(struct SeTransform* trf, struct SeFloat3 scale)
{
    struct SeFloat4x4 tr = se_f4x4_translation(se_t_get_position(*trf));
    struct SeFloat4x4 rt = se_f4x4_rotation(se_t_get_rotation(*trf)); 
    struct SeFloat4x4 sc = se_f4x4_scale(scale);
    trf->matrix = se_f4x4_mul_f4x4(tr, se_f4x4_mul_f4x4(rt, sc));
}
