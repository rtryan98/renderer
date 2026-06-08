// SHADER DEF trace_shadow_rays
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "shared/shared_resources.h"
#include "shared/scene_shared_types.h"

[shader("compute")]
[numthreads(8,4,1)]
void main(uint2 thread_id : SV_DispatchThreadID, uint2 group_id : SV_GroupID, uint group_idx : SV_GroupIndex)
{
    Scene_Info scene_info = rhi::uni::buf_load<Scene_Info>(REN_GLOBAL_SCENE_INFORMATION_BUFFER);

    // Initialize the Ray
    RayDesc sun_shadow_ray = {
        float3(0., 0., 0.),
        0.05,
        -scene_info.sun_direction,
        500.0
    };

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(
        rhi::get_rtas(scene_info.tlas),
        0,
        0xFF,
        sun_shadow_ray
    );

    // Trace
    while(q.Proceed());

    bool hit = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    uint mask = WaveActiveBitOr(hit * group_idx);

    if (WaveIsFirstLane())
    {

    }
}
