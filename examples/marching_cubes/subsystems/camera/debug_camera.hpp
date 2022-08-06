#ifndef _DEBUG_CAMERA_HPP_
#define _DEBUG_CAMERA_HPP_

#include "engine/se_math.hpp"
#include "engine/subsystems/se_window_subsystem.hpp"

struct DebugCamera
{
    SeFloat4x4  trf;
    int64_t     lastMouseX;
    int64_t     lastMouseY;
    bool        isRbmPressed;
};

void debug_camera_construct(DebugCamera* camera, SeFloat3 pos)
{
    *camera =
    {
        .trf            = float4x4::mul
        (
            float4x4::from_position(pos),
            float4x4::look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 })
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
    SeFloat3 position       = float4x4::get_position(camera->trf);
    const SeFloat3 forward  = float4x4::get_forward(camera->trf);
    const SeFloat3 up       = float4x4::get_up(camera->trf);
    const SeFloat3 right    = float4x4::get_right(camera->trf);
    const float add = (win::is_keyboard_button_pressed(input, SE_CTRL) ? 10.0f : 2.0f) * dt;
    if (win::is_keyboard_button_pressed(input, SE_W)) position = position + (forward * add);
    if (win::is_keyboard_button_pressed(input, SE_S)) position = position - (forward * add);
    if (win::is_keyboard_button_pressed(input, SE_A)) position = position - (right * add);
    if (win::is_keyboard_button_pressed(input, SE_D)) position = position + (right * add);
    if (win::is_keyboard_button_pressed(input, SE_Q)) position = position - (up * add);
    if (win::is_keyboard_button_pressed(input, SE_E)) position = position + (up * add);
    //
    // Update rotation
    //
    SeFloat3 rotation = float4x4::get_rotation(camera->trf);
    if (win::is_mouse_button_pressed(input, SE_RMB))
    {
        const bool isJustPressed = !camera->isRbmPressed;
        const int64_t x = input->mouseX;
        const int64_t y = input->mouseY;
        if (!isJustPressed)
        {
            rotation.x += (float)(camera->lastMouseY - y) * dt * 5.0f;
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
    camera->trf = float4x4::mul
    (
        float4x4::from_position(position),
        float4x4::from_rotation(rotation)
    );
}

#endif