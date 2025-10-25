// SHADER DEF ibl_prefilter_diffuse
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"

#include "common/sampling.hlsli"
#include "common/cubemap_utils.hlsli"

#include "shared/ibl_shared_types.h"

DECLARE_PUSH_CONSTANTS(Prefilter_Diffuse_Irradiance_Push_Constants, pc);

float3 tangent_to_world(float3 value, float3 N)
{
    float3 up = abs(N.z) < 0.99999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent_x = normalize(cross(up, N));
    float3 tangent_y = cross(N, tangent_x);
    return tangent_x * value.x + tangent_y * value.y + value.z * N;
}

float3 prefilter_irradiance(float3 R)
{
    float3 N = R;

    float3 irradiance = 0.0;
    static const uint SAMPLE_COUNT = 1 << 12;
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float3 L = tangent_to_world(ren::sampling::hemisphere_cosine_weighted(i, SAMPLE_COUNT), N);
        float NdotL = saturate(dot(N, L));

        // Using lower mip level to forcibly reduce frequency.
        float3 cube_sample = rhi::uni::tex_sample_level_cube<float4>(pc.source_cubemap, pc.cubemap_sampler, L, 3.0).xyz;

        // Hack: clamping sample value in case sun is in environment map.
        // Assuming 1.0 = 1 nit here, so anything above 10000 nits is irrelevant for current displays.
        // For diffuse, this is clamped to a much lower value, as high nits mostly generally correspond to specular reflections.
        irradiance += NdotL * clamp(cube_sample, 0.0, 100.0);
    }

    return irradiance / (float) SAMPLE_COUNT;
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float2 uv = float2(id.xy) / float2(pc.image_size);
    uv = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;
    float3 R = ren::cubemap_utils::direction_from_uv_thread_id(uv, id);
    float3 irradiance = prefilter_irradiance(R);
    rhi::uni::tex_store_arr(pc.target_cubemap, id.xy, id.z, float4(irradiance, 1.0));
}
