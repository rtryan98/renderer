// SHADER DEF skybox
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/ibl_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shared/shared_resources.h"
#include "rhi/bindless.hlsli"
#include "constants.hlsli"

DECLARE_PUSH_CONSTANTS(Skybox_Push_Constants, pc);

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.image_size.x || id.y >= pc.image_size.y)
        return;

    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    float2 uv = float2(id.xy) / float2(pc.image_size);
    uv = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;
    float4 position = float4(uv, 0.0, 1.0);
    float3 direction = mul((float3x3) camera.camera_to_world, mul(camera.clip_to_camera, position).xyz);
    direction = normalize(direction.xzy);
    float depth = rhi::uni::tex_load<float>(pc.depth_buffer, id.xy);
    float4 color = rhi::uni::tex_sample_level_cube<float4>(pc.cubemap, REN_SAMPLER_ANISO_WRAP, direction.xyz, 0.0);
    if (depth == 1.0)
    {
        rhi::uni::tex_store(pc.target_image, id.xy, color);
    }
}
