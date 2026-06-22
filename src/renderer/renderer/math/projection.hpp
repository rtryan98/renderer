#pragma once

#include <glm/matrix.hpp>

namespace ren::math
{
glm::mat4 perspective_fov_reverse_z_rh_zo(float fov, float width, float height, float z_near, float z_far);

glm::mat4 infinite_perspective_fov_reverse_z_rh_zo(float fov, float width, float height, float zNear);

float unproject_infinite_reverse_z(float z, float near);
}
