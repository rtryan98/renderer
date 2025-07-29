#pragma once

#include <DirectXMath.h>
#include "renderer/input_codes.hpp"

// Thanks WinDef.h
#undef near
#undef far

namespace ren
{
class Input_State;

using namespace DirectX;

struct Camera_Data
{
    XMFLOAT4X4 proj;
    XMFLOAT4X4 view;
    XMFLOAT4X4 view_proj;
    XMFLOAT4 position;
};

struct Camera_Input_Mapping
{
    SDL_Scancode move_forward = SDL_SCANCODE_W;
    Key_Code move_backward = SDL_SCANCODE_S;
    Key_Code move_right = SDL_SCANCODE_A;
    Key_Code move_left = SDL_SCANCODE_D;
    Key_Code move_up = SDL_SCANCODE_E;
    Key_Code move_down = SDL_SCANCODE_Q;
    Key_Code sprint = SDL_SCANCODE_LSHIFT;
    Mouse_Button enable_rotate = Mouse_Button::Mouse_Left;
};

struct Fly_Camera
{
public:
    constexpr static XMFLOAT3 WORLD_UP = { .0f, .0f, 1.f };

    Camera_Input_Mapping input_map;

    float fov_y;
    float aspect;
    float near;
    float far;
    float sensitivity;
    float movement_speed;
    float pitch;
    float yaw;

    XMFLOAT3 position;
    XMFLOAT3 forward;
    XMFLOAT3 right;
    XMFLOAT3 up;

    Camera_Data camera_data;

    void update();
    void process_inputs(const Input_State& input_state, float dt);

private:
    void update_rotation(const Input_State& input_state);
    void update_position(const Input_State& input_state, float dt);
};
}
