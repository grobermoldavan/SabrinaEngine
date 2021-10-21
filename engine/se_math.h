#ifndef _SE_MATH_H_
#define _SE_MATH_H_

#define SE_PI 3.14159265358979323846
#define SE_EPSILON 0.0000001
#define SE_MAX_FLOAT 3.402823466e+38F

#define se_to_radians(degrees) (degrees * SE_PI / 180.0f)
#define se_to_degrees(radians) (radians * 180.0f / SE_PI)
#define se_is_equal_float(value1, value2) ((value1 > value2 ? value1 - value2 : value2 - value1) < SE_EPSILON)

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

float           se_f2_dot(struct SeFloat2 first, struct SeFloat2 second);
struct SeFloat2 se_f2_add(struct SeFloat2 first, struct SeFloat2 second);
struct SeFloat2 se_f2_sub(struct SeFloat2 first, struct SeFloat2 second);
struct SeFloat2 se_f2_mul(struct SeFloat2 vec, float value);
struct SeFloat2 se_f2_div(struct SeFloat2 vec, float value);
float           se_f2_len(struct SeFloat2 vec);
float           se_f2_len2(struct SeFloat2 vec);
struct SeFloat2 se_f2_normalized(struct SeFloat2 vec);

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

float           se_f3_dot(struct SeFloat3 first, struct SeFloat3 second);
struct SeFloat3 se_f3_add(struct SeFloat3 first, struct SeFloat3 second);
struct SeFloat3 se_f3_sub(struct SeFloat3 first, struct SeFloat3 second);
struct SeFloat3 se_f3_mul(struct SeFloat3 vec, float value);
struct SeFloat3 se_f3_div(struct SeFloat3 vec, float value);
float           se_f3_len(struct SeFloat3 vec);
float           se_f3_len2(struct SeFloat3 vec);
struct SeFloat3 se_f3_normalized(struct SeFloat3 vec);
struct SeFloat3 se_f3_cross(struct SeFloat3 a, struct SeFloat3 b);

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

float           se_f4_dot(struct SeFloat4 first, struct SeFloat4 second);
struct SeFloat4 se_f4_add(struct SeFloat4 first, struct SeFloat4 second);
struct SeFloat4 se_f4_sub(struct SeFloat4 first, struct SeFloat4 second);
struct SeFloat4 se_f4_mul(struct SeFloat4 vec, float value);
struct SeFloat4 se_f4_div(struct SeFloat4 vec, float value);
float           se_f4_len(struct SeFloat4 vec);
float           se_f4_len2(struct SeFloat4 vec);
struct SeFloat4 se_f4_normalized(struct SeFloat4 vec);
struct SeFloat4 se_f4_cross(struct SeFloat4 a, struct SeFloat4 b);

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
        struct SeFloat4 rows[4];
        float elements[16];
        float m[4][4];
    };
};

const struct SeFloat4x4 SE_F4X4_IDENTITY =
{
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

struct SeFloat4x4   se_f4x4_mul_f4x4(struct SeFloat4x4 first, struct SeFloat4x4 second);
struct SeFloat4x4   se_f4x4_mul_f(struct SeFloat4x4 mat, float value);
struct SeFloat4     se_f4x4_mul_f4(struct SeFloat4x4 mat, struct SeFloat4 value);
struct SeFloat4x4   se_f4x4_transposed(struct SeFloat4x4 mat);
float               se_f4x4_det(struct SeFloat4x4 mat);
struct SeFloat4x4   se_f4x4_inverted(struct SeFloat4x4 mat);
struct SeFloat4x4   se_f4x4_translation(struct SeFloat3 position);
struct SeFloat4x4   se_f4x4_rotation(struct SeFloat3 eulerAngles);
struct SeFloat4x4   se_f4x4_scale(struct SeFloat3 scale);

//
// Quaternion conversions are taken from www.euclideanspace.com
//
// @TODO :  optimize conversions
//
struct SeQuaternion
{
    union
    {
        struct
        {
            float real;
            struct SeFloat3 imaginary;
        };
        struct
        {
            float w, x, y, z;
        };
        float elements[4];
    };
};

const struct SeQuaternion SE_Q_IDENTITY =
{
    .real = 1.0f,
    .imaginary = { 0, 0, 0 }
};

struct SeAxisAngle
{
    struct SeFloat3 axis;
    float angle;
};

struct SeQuaternion se_q_conjugate(struct SeQuaternion quat); // inverted
struct SeQuaternion se_q_from_axis_angle(struct SeAxisAngle axisAngle);
struct SeAxisAngle  se_q_to_axis_angle(struct SeQuaternion quat);
struct SeQuaternion se_q_from_euler_angles(struct SeFloat3 angles);
struct SeFloat3     se_q_to_euler_angles(struct SeQuaternion quat);
struct SeQuaternion se_q_from_rotation_mat(struct SeFloat4x4 mat);
struct SeFloat4x4   se_q_to_rotation_mat(struct SeQuaternion quat);
struct SeQuaternion se_q_from_mat(struct SeFloat4x4 mat);
struct SeQuaternion se_q_add(struct SeQuaternion first, struct SeQuaternion second);
struct SeQuaternion se_q_sub(struct SeQuaternion first, struct SeQuaternion second);
struct SeQuaternion se_q_mul_q(struct SeQuaternion first, struct SeQuaternion second);
struct SeQuaternion se_q_mul_f(struct SeQuaternion quat, float scalar);
struct SeQuaternion se_q_div_q(struct SeQuaternion first, struct SeQuaternion second);
struct SeQuaternion se_q_div_f(struct SeQuaternion quat, float scalar);
float               se_q_len(struct SeQuaternion quat);
float               se_q_len2(struct SeQuaternion quat);
struct SeQuaternion se_q_normalized(struct SeQuaternion quat);

struct SeTransform
{
    struct SeFloat4x4 matrix;
};

const struct SeTransform SE_T_IDENTITY =
{
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    },
};

struct SeFloat3 se_t_get_position(struct SeTransform trf);
struct SeFloat3 se_t_get_rotation(struct SeTransform trf);
struct SeFloat3 se_t_get_scale(struct SeTransform trf);
struct SeFloat3 se_t_get_forward(struct SeTransform trf);
struct SeFloat3 se_t_get_right(struct SeTransform trf);
struct SeFloat3 se_t_get_up(struct SeTransform trf);
struct SeFloat3 se_t_get_inverted_scale(struct SeTransform trf);
void            se_t_set_position(struct SeTransform* trf, struct SeFloat3 position);
void            se_t_set_rotation(struct SeTransform* trf, struct SeFloat3 eulerAngles);
void            se_t_set_scale(struct SeTransform* trf, struct SeFloat3 scale);

#endif
