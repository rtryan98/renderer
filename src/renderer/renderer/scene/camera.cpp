#include "renderer/scene/camera.hpp"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"
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
    // camera_data.camera_to_clip = glm::infinitePerspectiveRH(glm::radians(fov_y), aspect, near_plane);
    camera_data.camera_to_clip = glm::perspectiveRH(glm::radians(fov_y), aspect, near_plane, far_plane);
    camera_data.world_to_camera = glm::lookAtRH(
        position,
        position + forward,
        up);
    camera_data.world_to_clip = camera_data.camera_to_clip * camera_data.world_to_camera;
    camera_data.position = { position.x, position.y, position.z, 1.0f };
    camera_data.clip_to_camera = glm::inverse(camera_data.camera_to_clip);
    camera_data.camera_to_world = glm::inverse(camera_data.world_to_camera);
    camera_data.clip_to_world = glm::inverse(camera_data.world_to_clip);
}

void Fly_Camera::process_inputs(const Input_State& input_state, const float dt)
{
    update_rotation(input_state);
    update_position(input_state, dt);
}

bool Fly_Camera::box_in_frustum(const glm::vec3& min, const glm::vec3& max) const
{
    auto wtc_t = glm::transpose(camera_data.world_to_clip);
    auto planes = std::to_array({
        (wtc_t[3] + wtc_t[0]), // left
        (wtc_t[3] - wtc_t[0]), // right
        (wtc_t[3] + wtc_t[1]), // bottom
        (wtc_t[3] - wtc_t[1]), // top
        (wtc_t[3] + wtc_t[2])  // near
    });

    for (const auto& p : planes)
    {
        if (glm::dot(p, glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0f &&
            glm::dot(p, glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0f)
        {
            return false;
        }
    }

    return true;
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
