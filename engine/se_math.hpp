#ifndef _SE_MATH_HPP_
#define _SE_MATH_HPP_

/*
    Sabrina Engine math library.

    Coordinate system is left handed.
    Positive X - right direction
    Positive Y - up direction
    Positive Z - forward direction
    Cross product is right handed (https://en.wikipedia.org/wiki/Cross_product)
*/

#include <math.h>

#define SE_PI 3.14159265358979323846f
#define SE_EPSILON 0.0000001f
#define SE_MAX_FLOAT 3.402823466e+38F

#define se_to_radians(degrees) ((degrees) * SE_PI / 180.0f)
#define se_to_degrees(radians) ((radians) * 180.0f / SE_PI)
#define se_is_equal_float(value1, value2) (((value1) > (value2) ? (value1) - (value2) : (value2) - (value1)) < SE_EPSILON)

#define se_clamp(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define se_min(a, b) ((a) < (b) ? (a) : (b))
#define se_max(a, b) ((a) > (b) ? (a) : (b))
#define se_lerp(a, b, t) ((a) + (t) * ((b) - (a)))

struct SeFloat2
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
};

struct SeFloat3
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
};

struct SeFloat4
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
};

// https://www.cprogramming.com/tutorial/3d/quaternions.html
struct SeQuaternion
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
};

const SeQuaternion SE_Q_IDENTITY =
{
    .real = 1.0f,
    .imaginary = { 0, 0, 0 }
};

struct SeAxisAngle
{
    SeFloat3 axis;
    float angle;
};

struct SeFloat4x4
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
};

const SeFloat4x4 SE_F4X4_IDENTITY =
{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

namespace float2
{
    inline float    dot(SeFloat2 first, SeFloat2 second)    { return first.x * second.x + first.y * second.y; }
    inline SeFloat2 add(SeFloat2 first, SeFloat2 second)    { return { first.x + second.x, first.y + second.y }; }
    inline SeFloat2 sub(SeFloat2 first, SeFloat2 second)    { return { first.x - second.x, first.y - second.y }; }
    inline SeFloat2 mul(SeFloat2 vec, float value)          { return { vec.x * value, vec.y * value }; }
    inline SeFloat2 div(SeFloat2 vec, float value)          { return { vec.x / value, vec.y / value }; }
    inline float    len(SeFloat2 vec)                       { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
    inline float    len2(SeFloat2 vec)                      { return vec.x * vec.x + vec.y * vec.y; }
    inline SeFloat2 normalized(SeFloat2 vec)                { const float length = len(vec); return { vec.x / length, vec.y / length }; }
}

SeFloat2 operator + (SeFloat2 first, SeFloat2 second)           { return { first.x + second.x, first.y + second.y }; }
SeFloat2 operator - (SeFloat2 first, SeFloat2 second)           { return { first.x - second.x, first.y - second.y }; }
SeFloat2 operator * (SeFloat2 vec, float value)                 { return { vec.x * value, vec.y * value }; }
SeFloat2 operator / (SeFloat2 vec, float value)                 { return { vec.x / value, vec.y / value }; }
SeFloat2 operator * (float value, SeFloat2 vec)                 { return { vec.x * value, vec.y * value }; }
SeFloat2 operator / (float value, SeFloat2 vec)                 { return { vec.x / value, vec.y / value }; }

SeFloat2& operator += (SeFloat2& first, SeFloat2 second)        { first.x += second.x; first.y += second.y; return first; }
SeFloat2& operator -= (SeFloat2& first, SeFloat2 second)        { first.x -= second.x; first.y -= second.y; return first; }
SeFloat2& operator *= (SeFloat2& vec, float value)              { vec.x *= value; vec.y *= value; return vec; }
SeFloat2& operator /= (SeFloat2& vec, float value)              { vec.x /= value; vec.y /= value; return vec; }

namespace float3
{
    inline float    dot(const SeFloat3& first, const SeFloat3& second)  { return first.x * second.x + first.y * second.y + first.z * second.z; }
    inline SeFloat3 add(const SeFloat3& first, const SeFloat3& second)  { return { first.x + second.x, first.y + second.y, first.z + second.z }; }
    inline SeFloat3 sub(const SeFloat3& first, const SeFloat3& second)  { return { first.x - second.x, first.y - second.y, first.z - second.z }; }
    inline SeFloat3 mul(const SeFloat3& vec, float value)               { return { vec.x * value, vec.y * value, vec.z * value }; }
    inline SeFloat3 div(const SeFloat3& vec, float value)               { return { vec.x / value, vec.y / value, vec.z / value }; }
    inline float    len(const SeFloat3& vec)                            { return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z); }
    inline float    len2(const SeFloat3& vec)                           { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z; }
    inline SeFloat3 normalized(const SeFloat3& vec)                     { const float length = len(vec); return { vec.x / length, vec.y / length, vec.z / length }; }
    inline SeFloat3 cross(const SeFloat3& a, const SeFloat3& b)         { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
}

SeFloat3 operator + (const SeFloat3& first, const SeFloat3& second)     { return { first.x + second.x, first.y + second.y, first.z + second.z }; }
SeFloat3 operator - (const SeFloat3& first, const SeFloat3& second)     { return { first.x - second.x, first.y - second.y, first.z - second.z }; }
SeFloat3 operator * (const SeFloat3& vec, float value)                  { return { vec.x * value, vec.y * value, vec.z * value }; }
SeFloat3 operator / (const SeFloat3& vec, float value)                  { return { vec.x / value, vec.y / value, vec.z / value }; }
SeFloat3 operator * (float value, const SeFloat3& vec)                  { return { vec.x * value, vec.y * value, vec.z * value }; }
SeFloat3 operator / (float value, const SeFloat3& vec)                  { return { vec.x / value, vec.y / value, vec.z / value }; }

SeFloat3& operator += (SeFloat3& first, const SeFloat3& second)         { first.x += second.x; first.y += second.y; first.z += second.z; return first; }
SeFloat3& operator -= (SeFloat3& first, const SeFloat3& second)         { first.x -= second.x; first.y -= second.y; first.z -= second.z; return first; }
SeFloat3& operator *= (SeFloat3& vec, float value)                      { vec.x *= value; vec.y *= value; vec.z *= value; return vec; }
SeFloat3& operator /= (SeFloat3& vec, float value)                      { vec.x /= value; vec.y /= value; vec.z /= value; return vec; }

namespace float4
{
    inline float    dot(const SeFloat4& first, const SeFloat4& second)  { return first.x * second.x + first.y * second.y + first.z * second.z + first.w * second.w; }
    inline SeFloat4 add(const SeFloat4& first, const SeFloat4& second)  { return { first.x + second.x, first.y + second.y, first.z + second.z, first.w + second.w }; }
    inline SeFloat4 sub(const SeFloat4& first, const SeFloat4& second)  { return { first.x - second.x, first.y - second.y, first.z - second.z, first.w - second.w }; }
    inline SeFloat4 mul(const SeFloat4& vec, float value)               { return { vec.x * value, vec.y * value, vec.z * value, vec.w * value }; }
    inline SeFloat4 div(const SeFloat4& vec, float value)               { return { vec.x / value, vec.y / value, vec.z / value, vec.w / value }; }
    inline float    len(const SeFloat4& vec)                            { return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }
    inline float    len2(const SeFloat4& vec)                           { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w; }
    inline SeFloat4 normalized(const SeFloat4& vec)                     { const float length = len(vec); return { vec.x / length, vec.y / length, vec.z / length, vec.w / length }; }
}

SeFloat4 operator + (const SeFloat4& first, const SeFloat4& second)     { return { first.x + second.x, first.y + second.y, first.z + second.z, first.w + second.w }; }
SeFloat4 operator - (const SeFloat4& first, const SeFloat4& second)     { return { first.x - second.x, first.y - second.y, first.z - second.z, first.w - second.w }; }
SeFloat4 operator * (const SeFloat4& vec, float value)                  { return { vec.x * value, vec.y * value, vec.z * value, vec.w * value }; }
SeFloat4 operator / (const SeFloat4& vec, float value)                  { return { vec.x / value, vec.y / value, vec.z / value, vec.w / value }; }
SeFloat4 operator * (float value, const SeFloat4& vec)                  { return { vec.x * value, vec.y * value, vec.z * value, vec.w * value }; }
SeFloat4 operator / (float value, const SeFloat4& vec)                  { return { vec.x / value, vec.y / value, vec.z / value, vec.w / value }; }

SeFloat4& operator += (SeFloat4& first, const SeFloat4& second)         { first.x += second.x; first.y += second.y; first.z += second.z; first.w += second.w; return first; }
SeFloat4& operator -= (SeFloat4& first, const SeFloat4& second)         { first.x -= second.x; first.y -= second.y; first.z -= second.z; first.w -= second.w; return first; }
SeFloat4& operator *= (SeFloat4& vec, float value)                      { vec.x *= value; vec.y *= value; vec.z *= value; vec.w *= value; return vec; }
SeFloat4& operator /= (SeFloat4& vec, float value)                      { vec.x /= value; vec.y /= value; vec.z /= value; vec.w /= value; return vec; }

//
// Quaternion conversions are taken from www.euclideanspace.com
//
// @TODO :  optimize conversions
//
namespace quaternion
{
    inline SeQuaternion conjugate(const SeQuaternion& quat) // inverted
    {
        return
        {
            quat.real,
            float3::mul(quat.imaginary, -1.0f)
        };
    }

    inline SeQuaternion from_axis_angle(const SeAxisAngle& axisAngle)
    {
        const float halfAngle = se_to_radians(axisAngle.angle) * 0.5f;
        const float halfAngleSin = sinf(halfAngle);
        return
        {
            .w = cosf(halfAngle),
            .x = axisAngle.axis.x * halfAngleSin,
            .y = axisAngle.axis.y * halfAngleSin,
            .z = axisAngle.axis.z * halfAngleSin
        };
    }

    inline SeAxisAngle to_axis_angle(const SeQuaternion& quat)
    {
        const float invSqrt = 1.0f / sqrtf(1.0f - quat.w * quat.w);
        return
        {
            quat.x * invSqrt,
            quat.y * invSqrt,
            quat.z * invSqrt,
            se_to_degrees(2.0f * acosf(quat.w))
        };
    }

    inline SeQuaternion from_euler_angles(const SeFloat3& angles)
    {
        const float halfY = se_to_radians(angles.y) * 0.5f;
        const float halfZ = se_to_radians(angles.z) * 0.5f;
        const float halfX = se_to_radians(angles.x) * 0.5f;
        const float c1 = cosf(halfY);
        const float s1 = sinf(halfY);
        const float c2 = cosf(halfZ);
        const float s2 = sinf(halfZ);
        const float c3 = cosf(halfX);
        const float s3 = sinf(halfX);
        const float c1c2 = c1 * c2;
        const float s1s2 = s1 * s2;
        return
        {
            .w = c1c2 * c3 - s1s2 * s3,
            .x = c1c2 * s3 + s1s2 * c3,
            .y = s1 * c2 * c3 + c1 * s2 * s3,
            .z = c1 * s2 * c3 - s1 * c2 * s3
        };
    }

    SeFloat3 to_euler_angles(const SeQuaternion& quat)
    {
        // https://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/index.htm
        const float test = quat.x * quat.y + quat.z * quat.w;
        if (test > (0.5f - SE_EPSILON))
        {
            // singularity at north pole
            const float heading = 2.0f * atan2f(quat.x, quat.w);
            const float attitude = SE_PI * 0.5f;
            const float bank = 0.0f;
            return
            {
                se_to_degrees(bank),
                se_to_degrees(heading),
                se_to_degrees(attitude)
            };
        }
        if (test < (SE_EPSILON - 0.5f))
        {
            // singularity at south pole
            const float heading = -2.0f * atan2f(quat.x, quat.w);
            const float attitude = -SE_PI * 0.5f;
            const float bank = 0.0f;
            return
            {
                se_to_degrees(bank),
                se_to_degrees(heading),
                se_to_degrees(attitude)
            };
        }
        const float sqx = quat.x * quat.x;
        const float sqy = quat.y * quat.y;
        const float sqz = quat.z * quat.z;
        const float heading = atan2f(2.0f * quat.y * quat.w - 2.0f * quat.x * quat.z, 1.0f - 2.0f * sqy - 2.0f * sqz);
        const float attitude = asinf(2.0f * test);
        const float bank = atan2f(2.0f * quat.x * quat.w - 2.0f * quat.y * quat.z, 1.0f - 2.0f * sqx - 2.0f * sqz);
        return
        {
            se_to_degrees(bank),
            se_to_degrees(heading),
            se_to_degrees(attitude)
        };
    }

    SeQuaternion from_rotation_mat(const SeFloat4x4& mat)
    {
        const float tr = mat._00 + mat._11 + mat._22;
        if (!se_is_equal_float(tr, 0.0f) && tr > 0.0f)
        {
            float S = sqrtf(tr + 1.0f) * 2.0f;
            return
            {
                .w = 0.25f * S,
                .x = (mat._21 - mat._12) / S,
                .y = (mat._02 - mat._20) / S,
                .z = (mat._10 - mat._01) / S
            };
        }
        else if ((mat._00 > mat._11) && (mat._00 > mat._22))
        {
            float S = sqrtf(1.0f + mat._00 - mat._11 - mat._22) * 2.0f;
            return
            {
                .w = (mat._21 - mat._12) / S,
                .x = 0.25f * S,
                .y = (mat._01 + mat._10) / S,
                .z = (mat._02 + mat._20) / S
            };
        }
        else if (mat._11 > mat._22)
        {
            float S = sqrtf(1.0f + mat._11 - mat._00 - mat._22) * 2.0f;
            return
            {
                .w = (mat._02 - mat._20) / S,
                .x = (mat._01 + mat._10) / S,
                .y = 0.25f * S,
                .z = (mat._12 + mat._21) / S
            };
        }
        else
        {
            float S = sqrtf(1.0f + mat._22 - mat._00 - mat._11) * 2.0f;
            return
            {
                .w = (mat._10 - mat._01) / S,
                .x = (mat._02 + mat._20) / S,
                .y = (mat._12 + mat._21) / S,
                .z = 0.25f * S
            };
        }
    }

    inline SeFloat4x4 to_rotation_mat(const SeQuaternion& quat)
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
        return
        {
            m00, m01, m02, 0,
            m10, m11, m12, 0,
            m20, m21, m22, 0,
            0  , 0  , 0  , 1
        };
    }

    SeQuaternion from_mat(const SeFloat4x4& mat)
    {
        SeFloat3 inversedScale =
        {
            float3::len({ mat._00, mat._01, mat._02 }),
            float3::len({ mat._10, mat._11, mat._12 }),
            float3::len({ mat._20, mat._21, mat._22 })
        };
        inversedScale.y = se_is_equal_float(inversedScale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.y;
        inversedScale.x = se_is_equal_float(inversedScale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.x;
        inversedScale.z = se_is_equal_float(inversedScale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / inversedScale.z;
        const SeFloat4x4 rotationMatrix =
        {
            mat._00 * inversedScale.x, mat._01 * inversedScale.y, mat._02 * inversedScale.z, 0.0f,
            mat._10 * inversedScale.x, mat._11 * inversedScale.y, mat._12 * inversedScale.z, 0.0f,
            mat._20 * inversedScale.x, mat._21 * inversedScale.y, mat._22 * inversedScale.z, 0.0f,
            0.0f                     , 0.0f                     , 0.0f                     , 1.0f
        };
        return from_rotation_mat(rotationMatrix);
    }

    inline SeQuaternion add(const SeQuaternion& first, const SeQuaternion& second)
    {
        return
        {
            first.real + second.real,
            float3::add(first.imaginary, second.imaginary)
        };
    }

    inline SeQuaternion sub(const SeQuaternion& first, const SeQuaternion& second)
    {
        return
        {
            first.real + second.real,
            float3::sub(first.imaginary, second.imaginary)
        };
    }

    inline SeQuaternion mul(const SeQuaternion& first, const SeQuaternion& second)
    {
        // https://www.cprogramming.com/tutorial/3d/quaternions.html
        return
        {
            first.real * second.real - first.imaginary.x * second.imaginary.x - first.imaginary.y * second.imaginary.y - first.imaginary.z * second.imaginary.z,
            {
                first.real * second.imaginary.x + first.imaginary.x * second.real         + first.imaginary.y * second.imaginary.z   - first.imaginary.z * second.imaginary.y,
                first.real * second.imaginary.y - first.imaginary.x * second.imaginary.z  + first.imaginary.y * second.real          + first.imaginary.z * second.imaginary.x,
                first.real * second.imaginary.z + first.imaginary.x * second.imaginary.y  - first.imaginary.y * second.imaginary.x   + first.imaginary.z * second.real
            }
        };
    }

    inline SeQuaternion mul(const SeQuaternion& quat, float scalar)
    {
        return
        {
            quat.real * scalar,
            float3::mul(quat.imaginary, scalar)
        };
    }

    inline SeQuaternion div(const SeQuaternion& first, const SeQuaternion& second)
    {
        return mul(first, conjugate(second));
    }

    inline SeQuaternion div(const SeQuaternion& quat, float scalar)
    {
        return
        {
            quat.real / scalar,
            float3::div(quat.imaginary, scalar)
        };
    }

    inline float len(const SeQuaternion& quat)
    {
        return sqrtf(quat.real * quat.real + float3::len2(quat.imaginary));
    }

    float len2(const SeQuaternion& quat)
    {
        return quat.real * quat.real + float3::len2(quat.imaginary);
    }

    inline SeQuaternion normalized(const SeQuaternion& quat)
    {
        const float length = len(quat);
        return
        {
            quat.real / length,
            float3::div(quat.imaginary, length)
        };
    }
}

namespace float4x4
{
    inline SeFloat4x4 mul(const SeFloat4x4& first, const SeFloat4x4& second)
    {
        return
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

    inline SeFloat4x4 mul(const SeFloat4x4& mat, float value)
    {
        return
        {
            mat.m[0][0] * value, mat.m[0][1] * value, mat.m[0][2] * value, mat.m[0][3] * value,
            mat.m[1][0] * value, mat.m[1][1] * value, mat.m[1][2] * value, mat.m[1][3] * value,
            mat.m[2][0] * value, mat.m[2][1] * value, mat.m[2][2] * value, mat.m[2][3] * value,
            mat.m[3][0] * value, mat.m[3][1] * value, mat.m[3][2] * value, mat.m[3][3] * value
        };
    }

    inline SeFloat4 mul(const SeFloat4x4& mat, const SeFloat4& value)
    {
        return
        {
            value.x * mat.m[0][0] + value.y * mat.m[0][1] + value.z * mat.m[0][2] + value.w * mat.m[0][3],
            value.x * mat.m[1][0] + value.y * mat.m[1][1] + value.z * mat.m[1][2] + value.w * mat.m[1][3],
            value.x * mat.m[2][0] + value.y * mat.m[2][1] + value.z * mat.m[2][2] + value.w * mat.m[2][3],
            value.x * mat.m[3][0] + value.y * mat.m[3][1] + value.z * mat.m[3][2] + value.w * mat.m[3][3]
        };
    }

    inline SeFloat4x4 transposed(const SeFloat4x4& mat)
    {
        return
        {
            mat._00, mat._10, mat._20, mat._30,
            mat._01, mat._11, mat._21, mat._31,
            mat._02, mat._12, mat._22, mat._32,
            mat._03, mat._13, mat._23, mat._33
        };
    }

    inline float det(const SeFloat4x4& mat)
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

    inline SeFloat4x4 inverted(const SeFloat4x4& mat)
    {
        // https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
        // https://github.com/willnode/N-Matrix-Programmer
        const float determinant = det(mat);
        if (se_is_equal_float(determinant, 0.0f))
        {
            // @TODO : add method which returns bool here
            // Can't invert
            return { };
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
        return
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

    inline SeFloat4x4 from_position(const SeFloat3& position)
    {
        return
        {
            1, 0, 0, position.x,
            0, 1, 0, position.y,
            0, 0, 1, position.z,
            0, 0, 0, 1,
        };
    }

    inline SeFloat4x4 from_rotation(const SeFloat3& eulerAngles)
    {
        const float cosPitch = cosf(se_to_radians(eulerAngles.x));
        const float sinPitch = sinf(se_to_radians(eulerAngles.x));
        const float cosYaw   = cosf(se_to_radians(eulerAngles.y));
        const float sinYaw   = sinf(se_to_radians(eulerAngles.y));
        const float cosRoll  = cosf(se_to_radians(eulerAngles.z));
        const float sinRoll  = sinf(se_to_radians(eulerAngles.z));
        const SeFloat4x4 pitch =
        {
            1, 0       , 0        , 0,
            0, cosPitch, -sinPitch, 0,
            0, sinPitch,  cosPitch, 0,
            0, 0       , 0        , 1
        };
        const SeFloat4x4 yaw =
        {
            cosYaw , 0, sinYaw, 0,
            0      , 1, 0     , 0,
            -sinYaw, 0, cosYaw, 0,
            0      , 0, 0     , 1
        };
        const SeFloat4x4 roll =
        {
            cosRoll, -sinRoll, 0, 0,
            sinRoll,  cosRoll, 0, 0,
            0      , 0       , 1, 0,
            0      , 0       , 0, 1
        };
        return mul(yaw, mul(roll, pitch));
    }

    inline SeFloat4x4 from_scale(const SeFloat3& scale)
    {
        return
        {
            scale.x , 0         , 0         , 0,
            0       , scale.y   , 0         , 0,
            0       , 0         , scale.z   , 0,
            0       , 0         , 0         , 1
        };
    }

    inline SeFloat3 get_position(const SeFloat4x4& trf)
    {
        return { trf._03, trf._13, trf._23 };
    }

    inline SeFloat3 get_rotation(const SeFloat4x4& trf)
    {
        return quaternion::to_euler_angles(quaternion::from_mat(trf));
    }

    inline SeFloat3 get_scale(const SeFloat4x4& trf)
    {
        return
        {
            float3::len({ trf._00, trf._01, trf._02 }),
            float3::len({ trf._10, trf._11, trf._12 }),
            float3::len({ trf._20, trf._21, trf._22 })
        };
    }

    inline SeFloat3 get_inverted_scale(const SeFloat4x4& trf)
    {
        const SeFloat3 scale = get_scale(trf);
        return
        {
            se_is_equal_float(scale.x, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.x,
            se_is_equal_float(scale.y, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.y,
            se_is_equal_float(scale.z, 0.0f) ? SE_MAX_FLOAT : 1.0f / scale.z
        };
    }

    inline SeFloat3 get_forward(const SeFloat4x4& trf)
    {
        return float3::normalized({ trf.m[0][2], trf.m[1][2], trf.m[2][2] });
    }

    inline SeFloat3 get_right(const SeFloat4x4& trf)
    {
        return float3::normalized({ trf.m[0][0], trf.m[1][0], trf.m[2][0] });
    }

    inline SeFloat3 get_up(const SeFloat4x4& trf)
    {
        return float3::normalized({ trf.m[0][1], trf.m[1][1], trf.m[2][1] });
    }

    SeFloat4x4 look_at(const SeFloat3& from, const SeFloat3& to, const SeFloat3& up)
    {
        const SeFloat3 forward      = float3::normalized(float3::sub(to, from));
        const SeFloat3 right        = float3::normalized(float3::cross(up, forward));
        const SeFloat3 correctedUp  = float3::normalized(float3::cross(forward, right));
        SeFloat4x4 rotation         = SE_F4X4_IDENTITY;
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
}

inline SeFloat4x4   operator * (const SeFloat4x4& first, const SeFloat4x4& second)  { return float4x4::mul(first, second); }
inline SeFloat4x4   operator * (const SeFloat4x4& mat, float value)                 { return float4x4::mul(mat, value); }
inline SeFloat4     operator * (const SeFloat4x4& mat, const SeFloat4& value)       { return float4x4::mul(mat, value); }

#endif
