// SHADER DEF ibl_prefilter_specular
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"

#include "common/sampling.hlsli"
#include "common/cubemap_utils.hlsli"

#include "shared/ibl_shared_types.h"
#include "common/pbr/pbr.hlsli"
#include "shared/shared_resources.h"

DECLARE_PUSH_CONSTANTS(Prefilter_Specular_Irradiance_Push_Constants, pc);

float3 tangent_to_world(float3 value, float3 N)
{
    float3 up = abs(N.z) < 0.99999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent_x = normalize(cross(up, N));
    float3 tangent_y = cross(N, tangent_x);
    return tangent_x * value.x + tangent_y * value.y + value.z * N;
}

float3 prefilter_irradiance(float3 R, float roughness)
{
    float3 N = R;
    float3 V = R;

    float3 irradiance = 0.0;
    float weight = 0.0;

    float solid_angle_total = 4.0 * ren::PI / (6.0 * pc.image_size.x * pc.image_size.y);

    for (uint i = 0; i < pc.samples; ++i)
    {
        float3 H = tangent_to_world(ren::sampling::hemisphere_ggx(i, pc.samples, roughness), N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));

        if (NdotL > 0.0)
        {
            // Importance sampling using mipmaps
            // https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
            float pdf = ren::pbr::D_GGX_ThrowbridgeReitz(NdotH, roughness) * 0.25;
            float solid_angle_sample = 1.0 / ((float) pc.samples * pdf);
            // Hack: clamping mip level to get rid of blocky artefacts when environment map contains high luminance values (e.g. sun)
            float mip_level = clamp(0.5 * log(solid_angle_sample / solid_angle_total) + 1.0, 0.0, 5.0);

            float3 cube_sample = rhi::uni::tex_sample_level_cube<float4>(pc.source_cubemap, REN_SAMPLER_LINEAR_WRAP, L, mip_level).xyz;

            // Hack: clamping sample value in case sun is in environment map.
            irradiance += NdotL * clamp(cube_sample, 0.0, 100.0);
            weight += NdotL;
        }
    }

    return irradiance / weight;
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float2 uv = float2(id.xy) / float2(pc.image_size);
    uv = 2.0 * float2(uv.x, 1.0 - uv.y) - 1.0;
    float3 R = ren::cubemap_utils::direction_from_uv_thread_id(uv, id).xzy;
    float3 irradiance = prefilter_irradiance(R, pc.roughness);
    rhi::uni::tex_store_arr(pc.target_cubemap, id.xy, id.z, float4(irradiance, 1.0));
}
