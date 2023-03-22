#ifndef _DEBUG_CAMERA_HPP_
#define _DEBUG_CAMERA_HPP_

#include "engine/se_engine.hpp"

struct DebugCamera
{
    SeFloat4x4  trf;
    SeMousePos  lastMouse;
    bool        isRbmPressed;
};

void debug_camera_construct(DebugCamera* camera, SeFloat3 pos)
{
    *camera =
    {
        .trf            = se_float4x4_mul
        (
            se_float4x4_from_position(pos),
            se_float4x4_look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 })
        ),
        .lastMouse      = { },
        .isRbmPressed   = false,
    };
}

void debug_camera_update(DebugCamera* camera, float dt)
{
    //
    // Update position
    //
    SeFloat3 position       = se_float4x4_get_position(camera->trf);
    const SeFloat3 forward  = se_float4x4_get_forward(camera->trf);
    const SeFloat3 up       = se_float4x4_get_up(camera->trf);
    const SeFloat3 right    = se_float4x4_get_right(camera->trf);
    const float add = (se_win_is_keyboard_button_pressed(SeKeyboard::CTRL) ? 10.0f : 2.0f) * dt;
    if (se_win_is_keyboard_button_pressed(SeKeyboard::W)) position = position + (forward * add);
    if (se_win_is_keyboard_button_pressed(SeKeyboard::S)) position = position - (forward * add);
    if (se_win_is_keyboard_button_pressed(SeKeyboard::A)) position = position - (right * add);
    if (se_win_is_keyboard_button_pressed(SeKeyboard::D)) position = position + (right * add);
    if (se_win_is_keyboard_button_pressed(SeKeyboard::Q)) position = position - (up * add);
    if (se_win_is_keyboard_button_pressed(SeKeyboard::E)) position = position + (up * add);
    //
    // Update rotation
    //
    SeFloat3 rotation = se_float4x4_get_rotation(camera->trf);
    if (se_win_is_mouse_button_pressed(SeMouse::RMB))
    {
        const bool isJustPressed = !camera->isRbmPressed;
        const SeMousePos mousePos = se_win_get_mouse_pos();
        if (!isJustPressed)
        {
            rotation.x += float(camera->lastMouse.y - mousePos.y) * dt * 5.0f;
            rotation.y += float(mousePos.x - camera->lastMouse.x) * dt * 5.0f;
        }
        camera->lastMouse = mousePos;
        camera->isRbmPressed = true;
    }
    else
    {
        camera->isRbmPressed = false;
    }
    //
    // Set updated position and rotation
    //
    camera->trf = se_float4x4_mul
    (
        se_float4x4_from_position(position),
        se_float4x4_from_rotation(rotation)
    );
}

#endif