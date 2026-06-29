#ifndef REN_RT_RAY_OFFSET_HLSLI
#define REN_RT_RAY_OFFSET_HLSLI

namespace ren
{
namespace rt
{
// Ray Tracing Gems, Ch. 6 (Wächter and Binder, "A Fast and Robust Method for Avoiding Self-Intersection")
namespace detail
{
static const float ORIGIN = rcp(32.);
static const float FLOAT_SCALE = rcp(65536.);
static const float INT_SCALE = 256.;
}

// p: Initial ray origin
// n: Geometric normal
float3 offset_ray(const float3 p, const float3 n)
{
    int3 of_i = int3(detail::INT_SCALE * n);
    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0.) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0.) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0.) ? -of_i.z : of_i.z))
    );
    return float3(
        abs(p.x) < detail::ORIGIN ? p.x + detail::FLOAT_SCALE * n.x : p_i.x,
        abs(p.y) < detail::ORIGIN ? p.y + detail::FLOAT_SCALE * n.y : p_i.y,
        abs(p.z) < detail::ORIGIN ? p.z + detail::FLOAT_SCALE * n.z : p_i.z
    );
}

float3 ray_dir_disk_point(const float3 dir, float3 tangent, float3 bitangent, const float2 disk_point)
{
    return normalize(dir + disk_point.x * tangent + disk_point.y * bitangent);
}
}
}

#endif
