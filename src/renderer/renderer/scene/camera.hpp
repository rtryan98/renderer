#pragma once

#include <glm/glm.hpp>
#include "renderer/input_codes.hpp"

namespace ren
{
class Input_State;

struct Camera_Data
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 view_proj;
    glm::vec4 position;
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
    constexpr static glm::vec3 WORLD_UP = { .0f, .0f, 1.f };

    Camera_Input_Mapping input_map;

    float fov_y;
    float aspect;
    float near_plane;
    float sensitivity;
    float movement_speed;
    float pitch;
    float yaw;

    glm::vec3 position{};
    glm::vec3 forward{};
    glm::vec3 right{};
    glm::vec3 up{};

    Camera_Data camera_data;

    void update();
    void process_inputs(const Input_State& input_state, float dt);

private:
    void update_rotation(const Input_State& input_state);
    void update_position(const Input_State& input_state, float dt);
};
}
