#ifndef _DEBUG_CAMERA_HPP_
#define _DEBUG_CAMERA_HPP_

#include "engine/engine.hpp"

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
        .trf            = float4x4::mul
        (
            float4x4::from_position(pos),
            float4x4::look_at({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 })
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
    SeFloat3 position       = float4x4::get_position(camera->trf);
    const SeFloat3 forward  = float4x4::get_forward(camera->trf);
    const SeFloat3 up       = float4x4::get_up(camera->trf);
    const SeFloat3 right    = float4x4::get_right(camera->trf);
    const float add = (win::is_keyboard_button_pressed(SeKeyboard::CTRL) ? 10.0f : 2.0f) * dt;
    if (win::is_keyboard_button_pressed(SeKeyboard::W)) position = position + (forward * add);
    if (win::is_keyboard_button_pressed(SeKeyboard::S)) position = position - (forward * add);
    if (win::is_keyboard_button_pressed(SeKeyboard::A)) position = position - (right * add);
    if (win::is_keyboard_button_pressed(SeKeyboard::D)) position = position + (right * add);
    if (win::is_keyboard_button_pressed(SeKeyboard::Q)) position = position - (up * add);
    if (win::is_keyboard_button_pressed(SeKeyboard::E)) position = position + (up * add);
    //
    // Update rotation
    //
    SeFloat3 rotation = float4x4::get_rotation(camera->trf);
    if (win::is_mouse_button_pressed(SeMouse::RMB))
    {
        const bool isJustPressed = !camera->isRbmPressed;
        const SeMousePos mousePos = win::get_mouse_pos();
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
    camera->trf = float4x4::mul
    (
        float4x4::from_position(position),
        float4x4::from_rotation(rotation)
    );
}

#endif