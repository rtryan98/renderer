// SHADER DEF g_buffer_resolve
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/g_buffer_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "common/color/color_spaces.hlsli"
#include "util.hlsli"
#include "common/pbr/lighting.hlsli"

DECLARE_PUSH_CONSTANTS(G_Buffer_Resolve_Push_Constants, pc);

float3 position_from_depth(GPU_Camera_Data camera, float2 uv, float depth)
{
    float4 position = float4(uv * 2.0 - 1.0, depth, 1.0); // NDC
    position.y *= -1;
    position = mul(camera.clip_to_world, position);
    return position.xyz / position.w;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.width || id.y >= pc.height)
        return;
    float2 uv = (0.5 + float2(id.xy)) / float2(pc.width, pc.height);

    float depth = rhi::uni::tex_sample_level<float>(pc.depth, pc.texture_sampler, uv, 0.).x;
    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    float3 position = position_from_depth(camera, uv, depth);
    float3 V = normalize(camera.position.xyz - position);

    ren::pbr::Surface surface;
    surface.position = position;
    surface.albedo = ren::color::spaces::Rec709_Rec2020(rhi::uni::tex_sample_level<float4>(pc.albedo, pc.texture_sampler, uv, 0.).xyz);
    surface.normal = rhi::uni::tex_sample_level<float4>(pc.normals, pc.texture_sampler, uv, 0.).xyz;
    float2 metallic_roughness = rhi::uni::tex_sample_level<float2>(pc.metallic_roughness, pc.texture_sampler, uv, 0.);
    surface.metallic = metallic_roughness.x;
    surface.roughness = metallic_roughness.y;

    float3 color = ren::pbr::evaluate_lights(V, surface);

    Scene_Info scene_info = rhi::uni::buf_load<Scene_Info>(REN_GLOBAL_SCENE_INFORMATION_BUFFER);

    RayDesc ray = {
        surface.position + 0.05 * surface.normal,
        0.05,
        -scene_info.sun_direction,
        500.0
    };

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(
        rhi::get_rtas(scene_info.tlas),
        0,
        0xFF,
        ray
    );

    q.Proceed();
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        color *= 0.;
    }

    float4 result = float4(color, 1.0);
    rhi::uni::tex_store(pc.resolve_target, id.xy, result);
}
