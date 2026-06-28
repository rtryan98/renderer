// SHADER DEF g_buffer_resolve
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/g_buffer_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shared/shared_resources.h"
#include "rhi/bindless.hlsli"
#include "common/color/color_spaces.hlsli"
#include "util.hlsli"
#include "common/pbr/lighting.hlsli"
#include "shaders/common/octahedron_encoding.hlsli"
#include "shaders/common/projection.hlsli"

DECLARE_PUSH_CONSTANTS(G_Buffer_Resolve_Push_Constants, pc);

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= pc.width || id.y >= pc.height)
        return;
    float2 uv = ren::uv_from_thread_id(id, uint2(pc.width, pc.height));

    float4 g_buffer_0 = rhi::tex_sample_level<float4>(pc.g_buffer_0, REN_SAMPLER_LINEAR_CLAMP, uv, 0.);
    float4 g_buffer_1 = rhi::tex_sample_level<float4>(pc.g_buffer_1, REN_SAMPLER_LINEAR_CLAMP, uv, 0.);
    float2 g_buffer_2 = rhi::tex_sample_level<float2>(pc.g_buffer_2, REN_SAMPLER_LINEAR_CLAMP, uv, 0.);
    // float2 g_buffer_3 = rhi::tex_sample_level<float2>(pc.g_buffer_3, REN_SAMPLER_LINEAR_CLAMP, uv, 0.); // unused
    float depth = rhi::tex_sample_level<float>(pc.depth, REN_SAMPLER_LINEAR_CLAMP, uv, 0.).x;
    
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera_buffer);

    ren::pbr::Surface surface;
    surface.position = ren::position_from_depth(camera.clip_to_world, uv, depth);
    surface.albedo = ren::color::spaces::Rec709_Rec2020(g_buffer_0.xyz);
    surface.normal = ren::oct_signed_decode(g_buffer_1.xyw);
    surface.metallic = g_buffer_2.x;
    surface.roughness = g_buffer_2.y;

    float3 V = normalize(camera.position.xyz - surface.position);
    float3 color = ren::pbr::evaluate_lights(V, surface);

    float4 result = float4(color, 1.0);
    rhi::tex_store(pc.resolve_target, id.xy, result);
}
