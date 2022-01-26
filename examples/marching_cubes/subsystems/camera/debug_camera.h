#ifndef _DEBUG_CAMERA_H_
#define _DEBUG_CAMERA_H_

#include "engine/se_math.h"
#include "engine/subsystems/se_window_subsystem.h"

typedef struct DebugCamera
{
    SeFloat4x4 trf;
    int64_t lastMouseX;
    int64_t lastMouseY;
    bool isRbmPressed;
} DebugCamera;

void debug_camera_construct(DebugCamera* camera, SeFloat3 pos)
{
    *camera = (DebugCamera)
    {
        .trf            = se_f4x4_mul_f4x4
        (
            se_f4x4_from_position(pos),
            se_look_at((SeFloat3){0}, (SeFloat3){ 0, 0, 1 }, (SeFloat3){ 0, 1, 0 })
        ),
        .lastMouseX     = 0,
        .lastMouseY     = 0,
        .isRbmPressed   = false,
    };
}

void debug_camera_update(DebugCamera* camera, const SeWindowSubsystemInput* input, float dt)
{
    //
    // Update position
    //
    SeFloat3 position       = se_f4x4_get_position(camera->trf);
    const SeFloat3 forward  = se_f4x4_get_forward(camera->trf);
    const SeFloat3 up       = se_f4x4_get_up(camera->trf);
    const SeFloat3 right    = se_f4x4_get_right(camera->trf);
    const float add = (se_is_keyboard_button_pressed(input, SE_CTRL) ? 10.0f : 2.0f) * dt;
    if (se_is_keyboard_button_pressed(input, SE_W)) position = se_f3_add(position, se_f3_mul(forward, add));
    if (se_is_keyboard_button_pressed(input, SE_S)) position = se_f3_sub(position, se_f3_mul(forward, add));
    if (se_is_keyboard_button_pressed(input, SE_A)) position = se_f3_sub(position, se_f3_mul(right, add));
    if (se_is_keyboard_button_pressed(input, SE_D)) position = se_f3_add(position, se_f3_mul(right, add));
    if (se_is_keyboard_button_pressed(input, SE_Q)) position = se_f3_sub(position, se_f3_mul(up, add));
    if (se_is_keyboard_button_pressed(input, SE_E)) position = se_f3_add(position, se_f3_mul(up, add));
    //
    // Update rotation
    //
    SeFloat3 rotation = se_f4x4_get_rotation(camera->trf);
    if (se_is_mouse_button_pressed(input, SE_RMB))
    {
        const bool isJustPressed = !camera->isRbmPressed;
        const int64_t x = input->mouseX;
        const int64_t y = input->mouseY;
        if (!isJustPressed)
        {
            rotation.x += (float)(y - camera->lastMouseY) * dt * 5.0f;
            rotation.y += (float)(x - camera->lastMouseX) * dt * 5.0f;
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
    camera->trf = se_f4x4_mul_f4x4
    (
        se_f4x4_from_position(position),
        se_f4x4_from_rotation(rotation)
    );
}

#endif