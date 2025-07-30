#include "renderer/scene/camera.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "renderer/window.hpp"

namespace ren
{
void Fly_Camera::update()
{
    forward = glm::normalize(glm::vec3{
        glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch)),
        glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)),
        glm::sin(glm::radians(pitch))
    });
    right = glm::normalize(glm::cross(WORLD_UP, forward));
    up = glm::normalize(glm::cross(forward, right));
    camera_data.proj = glm::infinitePerspectiveRH(glm::radians(fov_y), aspect, near_plane);
    camera_data.proj = glm::perspectiveRH(glm::radians(fov_y), aspect, near_plane, 1000.f);
    camera_data.view = glm::lookAtRH(
        position,
        position + forward,
        up);
    camera_data.view_proj = camera_data.proj * camera_data.view;
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
        yaw += sensitivity * mouse_delta.x;
        if (yaw > 360.0f)
        {
            yaw -= 360.0f;
        }
        if (yaw < 0.0f)
        {
            yaw += 360.0f;
        }
        pitch -= sensitivity * mouse_delta.y;
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }
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

    glm::vec3 movement = { 0.0f, 0.0f, 0.0f };

    if (inp_forward && inp_back)
        ;
    else if (inp_forward)
    {
        movement += forward;
    }
    else if (inp_back)
    {
        movement -= forward;
    }

    if (inp_left && inp_right)
        ;
    else if (inp_right)
    {
        movement += right;
    }
    else if (inp_left)
    {
        movement -= right;
    }

    if (inp_up && inp_down)
        ;
    else if (inp_up)
    {
        movement += WORLD_UP;
    }
    else if (inp_down)
    {
        movement -= WORLD_UP;
    }

    if (glm::length(movement) > 0.0f)
    {
        position += speed * glm::normalize(movement);
    }
}
}
