#pragma once

#include <glm/glm.hpp>
#include "renderer/input_codes.hpp"

namespace ren
{
class Input_State;

struct Camera_Data
{
    glm::mat4 camera_to_clip;
    glm::mat4 world_to_camera;
    glm::mat4 world_to_clip;
    glm::mat4 clip_to_camera;
    glm::mat4 camera_to_world;
    glm::mat4 clip_to_world;
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
    float far_plane;
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

    [[nodiscard]] bool box_in_frustum(const glm::vec3& min, const glm::vec3& max) const;

private:
    void update_rotation(const Input_State& input_state);
    void update_position(const Input_State& input_state, float dt);
};
}
