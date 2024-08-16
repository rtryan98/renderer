#include "renderer/scene/camera.hpp"

#include "renderer/window.hpp"

namespace ren
{
void Fly_Camera::update()
{
    XMStoreFloat4x4(&camera_data.proj, XMMatrixPerspectiveFovRH(
        XMConvertToRadians(fov_y), aspect, near, far));
    XMStoreFloat4x4(&camera_data.view, XMMatrixLookAtRH(
        XMLoadFloat3(&position),
        XMVectorAdd(
            XMLoadFloat3(&position),
            XMLoadFloat3(&forward)),
        XMLoadFloat3(&up)));
    XMStoreFloat4x4(&camera_data.view_proj, XMMatrixMultiply(
        XMLoadFloat4x4(&camera_data.view), XMLoadFloat4x4(&camera_data.proj)));
    camera_data.position = { position.x, position.y, position.z, 0.0f };
}

void Fly_Camera::process_inputs(const Input_State& input_state, float dt)
{
    update_rotation(input_state);
    update_position(input_state, float(dt));
}

void Fly_Camera::update_rotation(const Input_State& input_state)
{
    if (input_state.is_mouse_pressed(input_map.enable_rotate))
    {
        auto mouse_delta = input_state.get_mouse_pos_delta();
        yaw -= sensitivity * mouse_delta.x;
        if (yaw > 360.0f)
        {
            yaw -= 360.0f;
        }
        if (yaw < 0.0f)
        {
            yaw += 360.0f;
        }
        pitch -= sensitivity * mouse_delta.y;
        pitch = XMMax(XMMin(pitch, 89.0f), -89.0f);
    }
    forward = {
        XMScalarCos(XMConvertToRadians(yaw)) * XMScalarCos(XMConvertToRadians(pitch)),
        XMScalarSin(XMConvertToRadians(yaw)) * XMScalarCos(XMConvertToRadians(pitch)),
        XMScalarSin(XMConvertToRadians(pitch)),
    };
    XMStoreFloat3(&forward,
        XMVector3Normalize(XMLoadFloat3(&forward)));
    XMStoreFloat3(&right,
        XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&forward), XMLoadFloat3(&WORLD_UP))));
    XMStoreFloat3(&up,
        XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&right), XMLoadFloat3(&forward))));
}

void Fly_Camera::update_position(const Input_State& input_state, float dt)
{
    bool inp_forward = input_state.is_key_pressed(input_map.move_forward);
    bool inp_left = input_state.is_key_pressed(input_map.move_left);
    bool inp_back = input_state.is_key_pressed(input_map.move_backward);
    bool inp_right = input_state.is_key_pressed(input_map.move_right);
    bool inp_down = input_state.is_key_pressed(input_map.move_down);
    bool inp_up = input_state.is_key_pressed(input_map.move_up);

    float speed = movement_speed * dt;
    if (input_state.is_key_pressed(input_map.sprint))
    {
        speed *= 2.0f;
    }

    XMFLOAT3 movement = { 0.0f, 0.0f, 0.0f };

    if (inp_forward && inp_back)
        ;
    else if (inp_forward)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(XMLoadFloat3(&forward), XMLoadFloat3(&movement)));
    }
    else if (inp_back)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(-1.0f * XMLoadFloat3(&forward), XMLoadFloat3(&movement)));
    }

    if (inp_left && inp_right)
        ;
    else if (inp_right)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(XMLoadFloat3(&right), XMLoadFloat3(&movement)));
    }
    else if (inp_left)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(-1.0f * XMLoadFloat3(&right), XMLoadFloat3(&movement)));
    }

    if (inp_up && inp_down)
        ;
    else if (inp_up)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(XMLoadFloat3(&WORLD_UP), XMLoadFloat3(&movement)));
    }
    else if (inp_down)
    {
        XMStoreFloat3(&movement,
            XMVectorAdd(-1.0f * XMLoadFloat3(&WORLD_UP), XMLoadFloat3(&movement)));
    }

    XMStoreFloat3(&position,
        XMVectorAdd(XMLoadFloat3(&position), speed * XMVector3Normalize(XMLoadFloat3(&movement))));
}
}
