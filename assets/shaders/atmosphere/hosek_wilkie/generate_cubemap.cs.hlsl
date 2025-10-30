// SHADER DEF hosek_wilkie_generate_cubemap
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/hosek_wilkie_shared_types.h"
#include "rhi/bindless.hlsli"
#include "constants.hlsli"
#include "common/cubemap_utils.hlsli"
#include "common/color/color_spaces.hlsli"

DECLARE_PUSH_CONSTANTS(Hosek_Wilkie_Cubemap_Gen_Push_Constants, pc);

float3 hosek_wilkie(float cos_theta, float gamma, float cos_gamma)
{
    Hosek_Wilkie_Parameters cfg = rhi::uni::buf_load<Hosek_Wilkie_Parameters>(pc.parameters_buffer);

    float3 expm = exp(cfg.values[4].xyz * gamma);
    float3 raym = cos_gamma * cos_gamma;
    float zenith = sqrt(cos_theta);
    float3 miem = (1. + raym) / pow(1. + cfg.values[8].xyz * cfg.values[8].xyz - 2. * cos_gamma * cfg.values[8].xyz, 1.5);

    float3 result = (1. + cfg.values[0].xyz * exp(cfg.values[1].xyz / max(cos_theta, 0.01)) ) *
        (cfg.values[2].xyz + cfg.values[3].xyz * expm +
            cfg.values[5].xyz * raym + cfg.values[6].xyz * miem +
            cfg.values[7].xyz * zenith);
    result *= cfg.radiance.xyz;

    return result / 100.;
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float2 uv = float2(id.xy) / float(pc.image_size - 1);
    uv = 2. * float2(uv.x, 1.0 - uv.y) - 1.0;
    float3 direction = ren::cubemap_utils::direction_from_uv_thread_id(uv, id);

    float3 sun_direction = -pc.sun_direction.xyz;

    float cos_theta = saturate(direction.z);
    float cos_gamma = saturate(dot(direction, sun_direction));
    float gamma = acos(cos_gamma);
    float3 color = hosek_wilkie(cos_theta, gamma, cos_gamma);

    if (pc.use_xyz)
        color = ren::color::spaces::XYZ_Rec2020(color);
    else
        color = ren::color::spaces::Rec709_Rec2020(color);

    rhi::uni::tex_store_arr(pc.target_cubemap, id.xy, id.z, color);
}
