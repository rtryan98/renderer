#include "renderer/math/projection.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/reciprocal.hpp>

namespace ren::math
{
glm::mat4 perspective_fov_reverse_z_rh_zo(float fov, float width, float height, float z_near, float z_far)
{
    return glm::perspectiveFovRH_ZO(fov, width, height, z_far, z_near);
}

glm::mat4 infinite_perspective_fov_reverse_z_rh_zo(float fov, float width, float height, float zNear)
{
    const float h = glm::cot(0.5f * fov);
    const float w = h * height / width;
    glm::mat4 result = glm::zero<glm::mat4>();
    result[0][0] = w;
    result[1][1] = h;
    result[2][2] = 0.f;
    result[2][3] = -1.f;
    result[3][2] = zNear;
    return result;
}

float unproject_infinite_reverse_z(float z, float near)
{
    return near / z;
}
}
