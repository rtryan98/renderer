// SHADER DEF rt_shadows_trace
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "shared/shared_resources.h"
#include "shared/camera_shared_types.h"
#include "shared/scene_shared_types.h"
#include "shared/rt_shadows_shared_types.h"
#include "shaders/common/octahedron_encoding.hlsli"
#include "shaders/common/projection.hlsli"
#include "shaders/common/rt/ray_offset.hlsli"
#include "shaders/common/rt/ray_query.hlsli"

DECLARE_PUSH_CONSTANTS(RT_Shadows_Trace_Push_Constants, pc);

static const uint GROUP_THREAD_COUNT_X = 8;
static const uint GROUP_THREAD_COUNT_Y = 4;

[shader("compute")]
[numthreads(GROUP_THREAD_COUNT_X, GROUP_THREAD_COUNT_Y, 1)]
void main(uint2 thread_id: SV_DispatchThreadID, uint group_index : SV_GroupIndex, uint2 group_id: SV_GroupID, uint group_idx: SV_GroupIndex)
{
    if (thread_id.x >= pc.image_size.x || thread_id.y >= pc.image_size.y)
        return;

    float2 uv = ren::uv_from_thread_id(thread_id, pc.image_size);

    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    float depth = rhi::tex_sample_level<float>(pc.depth_texture, REN_SAMPLER_LINEAR_CLAMP, uv, 0.);

    if (depth == 0.0) // No need to trace a ray with no source geometry.
        return;

    Scene_Info scene_info = rhi::buf_load<Scene_Info>(REN_GLOBAL_SCENE_INFORMATION_BUFFER);
    float4 g_buffer_1 = rhi::tex_sample_level<float4>(pc.g_buffer_1_texture, REN_SAMPLER_LINEAR_CLAMP, uv, 0.);

    float3 position = ren::position_from_depth(camera.clip_to_world, uv, depth);
    float3 V = normalize(camera.position.xyz - position);
    float3 N = ren::oct_signed_decode(g_buffer_1.xyw);
    float3 ray_origin = ren::rt::offset_ray(position, N);

    bool is_visible = false;
    if (dot(-scene_info.sun_direction, V) > 0.)
    {
        is_visible = ren::rt::query_visibility(
            rhi::get_rtas(scene_info.tlas),
            ray_origin,
            -scene_info.sun_direction,
            0.05,
            1000.);
    }

    uint result = WaveActiveBitOr(uint(is_visible) << group_index);

    // Assumes WaveSize == 32
    if (WaveIsFirstLane())
    {
        rhi::tex_store(pc.visibility_output_texture, group_id, result);
    }
}
