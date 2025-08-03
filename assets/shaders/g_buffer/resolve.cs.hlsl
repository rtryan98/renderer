#include "shared/g_buffer_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(G_Buffer_Resolve_Push_Constants, pc);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.width || id.y >= pc.height)
        return;
    float2 uv = id.xy / float2(pc.width, pc.height);
    float4 result = rhi::uni::tex_sample_level<float4>(pc.albedo, pc.texture_sampler, uv, 0.);
    rhi::uni::tex_store(pc.resolve_target, id.xy, result);
}
