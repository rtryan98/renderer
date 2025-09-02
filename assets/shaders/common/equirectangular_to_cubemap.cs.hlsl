#include "shared/ibl_shared_types.h"
#include "rhi/bindless.hlsli"
#include "constants.hlsli"
#include "common/cubemap_utils.hlsli"

DECLARE_PUSH_CONSTANTS(Equirectangular_To_Cubemap_Push_Constants, pc);

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float2 uv = float2(id.xy) / float2(pc.image_size);
    uv = uv * 2.0 - 1.0;
    float3 direction = ren::cubemap_utils::direction_from_uv_thread_id(uv, id);
    float theta = acos(direction.y);
    float phi = atan2(direction.z, direction.x);
    uv = float2((phi + ren::PI) / ren::TWO_PI, 1.0 - theta / ren::PI);
    rhi::uni::tex_store_arr(pc.target_cubemap, id.xy, id.z, rhi::uni::tex_sample_level<float4>(pc.source_image, pc.source_image_sampler, uv, 0.0));
}
