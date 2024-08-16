#pragma once

#include <DirectXMath.h>

namespace ren
{
using namespace DirectX;

struct Camera_Data
{
    XMFLOAT4X4 proj;
    XMFLOAT4X4 view;
    XMFLOAT4X4 view_proj;
    XMFLOAT4 position;
};

class Camera
{
public:


private:

};
}
