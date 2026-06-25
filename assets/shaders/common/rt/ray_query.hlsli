#ifndef REN_RT_RAY_QUERY_HLSLI
#define REN_RT_RAY_QUERY_HLSLI

namespace ren
{
namespace rt
{
float query_distance(RaytracingAccelerationStructure tlas,
    float3 ray_origin,
    float3 ray_direction,
    float t_min,
    float t_max,
    uint ray_flags = 0,
    uint instance_inclusion_mask = 0xFF)
{
    RayDesc ray = {
        ray_origin,
        t_min,
        ray_direction,
        t_max
    };
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    q.TraceRayInline(
        tlas,
        ray_flags,
        instance_inclusion_mask,
        ray);
    q.Proceed();
    if (q.CommittedStatus() != COMMITTED_NOTHING)
    {
        return q.CommittedRayT();
    }
    return t_max;
}

bool query_visibility(RaytracingAccelerationStructure tlas,
    float3 ray_origin,
    float3 ray_direction,
    float t_min,
    float t_max,
    uint ray_flags = 0,
    uint instance_inclusion_mask = 0xFF)
{
    RayDesc ray = {
        ray_origin,
        t_min,
        ray_direction,
        t_max
    };
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    q.TraceRayInline(
        tlas,
        ray_flags,
        instance_inclusion_mask,
        ray);
    q.Proceed();
    if (q.CommittedStatus() != COMMITTED_NOTHING)
    {
        return false;
    }
    return true;
}
}
}

#endif
