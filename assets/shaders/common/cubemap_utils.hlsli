#ifndef REN_CUBEMAP_UTILS
#define REN_CUBEMAP_UTILS

namespace ren
{
namespace cubemap_utils
{
float3 direction_from_uv_thread_id(float2 uv, uint3 id)
{
    float3 direction = 0.0;
    switch (id.z)
    {
    case 0:
        direction = float3(1.0, uv.y, -uv.x);
        break;
    case 1:
        direction = float3(-1.0, uv.y, uv.x);
        break;
    case 2:
        direction = float3(uv.x, -1.0, uv.y);
        break;
    case 3:
        direction = float3(uv.x, 1.0, -uv.y);
        break;
    case 4:
        direction = float3(uv.x, uv.y, 1.0);
        break;
    case 5:
        direction = float3(-uv.x, uv.y, -1.0);
        break;
    default:
        break;
    }
    return normalize(direction);
}
}
}

#endif
