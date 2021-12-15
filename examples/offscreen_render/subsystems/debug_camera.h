#ifndef _USER_APP_DEBUG_CAMERA_H_
#define _USER_APP_DEBUG_CAMERA_H_

#include "engine/se_math.h"
#include "engine/subsystems/se_window_subsystem.h"

typedef struct DebugCamera
{
    SeTransform trf;
    float fovDeg;
    float aspect;
    float nearPlane;
    float farPlane;
    int64_t lastMouseX;
    int64_t lastMouseY;
    uint32_t lastMouseWheel;
    bool isRbmPressed;
} DebugCamera;

void debug_camera_construct(DebugCamera* camera, float fovDeg, float aspect, float nearPlane, float farPlane)
{
    camera->trf = SE_T_IDENTITY;
    camera->fovDeg = fovDeg;
    camera->aspect = aspect;
    camera->nearPlane = nearPlane;
    camera->farPlane = farPlane;
    camera->isRbmPressed = false;
    se_t_look_at(&camera->trf, (SeFloat3){0}, (SeFloat3){ 0, 0, 1 }, (SeFloat3){ 0, 1, 0 });
}

void debug_camera_update(DebugCamera* camera, const SeWindowSubsystemInput* input, float dt)
{
    //
    // Update position
    //
    SeFloat3 position = se_t_get_position(&camera->trf);
    const SeFloat3 forward = se_t_get_forward(&camera->trf);
    const SeFloat3 up = se_t_get_up(&camera->trf);
    const SeFloat3 right = se_t_get_right(&camera->trf);
    const float add = (se_is_keyboard_button_pressed(input, SE_CTRL) ? 10.0f : 2.0f) * dt;
    if (se_is_keyboard_button_pressed(input, SE_W)) position = se_f3_add(position, se_f3_mul(forward, add));
    if (se_is_keyboard_button_pressed(input, SE_S)) position = se_f3_sub(position, se_f3_mul(forward, add));
    if (se_is_keyboard_button_pressed(input, SE_A)) position = se_f3_sub(position, se_f3_mul(right, add));
    if (se_is_keyboard_button_pressed(input, SE_D)) position = se_f3_add(position, se_f3_mul(right, add));
    if (se_is_keyboard_button_pressed(input, SE_Q)) position = se_f3_add(position, se_f3_mul(up, add));
    if (se_is_keyboard_button_pressed(input, SE_E)) position = se_f3_sub(position, se_f3_mul(up, add));
    //
    // Update rotation
    //
    SeFloat3 rotation = se_t_get_rotation(&camera->trf);
    if (se_is_mouse_button_pressed(input, SE_RMB))
    {
        const bool isJustPressed = !camera->isRbmPressed;
        const int64_t x = input->mouseX;
        const int64_t y = input->mouseY;
        if (!isJustPressed)
        {
            rotation.x -= (float)(y - camera->lastMouseY) * dt * 50.0f;
            rotation.y += (float)(x - camera->lastMouseX) * dt * 50.0f;
        }
        camera->lastMouseX = x;
        camera->lastMouseY = y;
        camera->isRbmPressed = true;
    }
    else
    {
        camera->isRbmPressed = false;
    }
    //
    // Set updated position and rotation
    //
    se_t_set_position(&camera->trf, position);
    se_t_set_rotation(&camera->trf, rotation);
}

SeFloat4x4 debug_camera_get_view_projection(DebugCamera* camera)
{
    return se_f4x4_mul_f4x4
    (
        se_perspective_projection(camera->fovDeg, camera->aspect, camera->nearPlane, camera->farPlane),
        se_f4x4_inverted(camera->trf.matrix)
    );
}

#endif
