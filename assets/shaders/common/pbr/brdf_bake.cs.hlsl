// SHADER DEF brdf_bake
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"

#include "common/sampling.hlsli"
#include "common/cubemap_utils.hlsli"

#include "shared/ibl_shared_types.h"
#include "common/pbr/pbr.hlsli"

DECLARE_PUSH_CONSTANTS(BRDF_LUT_Bake_Push_Constants, pc);

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float theta = max((float) id.x / (float) pc.image_size.x, 0.001);
    float roughness = (float) id.y / (float) pc.image_size.y;
    float3 V = float3(sqrt(1.0 - theta * theta), 0.0, theta);

    float2 AB = 0.0;
    static const uint SAMPLE_COUNT = 1 << 10;
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float3 H = ren::sampling::hemisphere_ggx(i, SAMPLE_COUNT, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));

        if (NdotL > 0.0)
        {
            float G = ren::pbr::G_GGX_ibl(NdotL, VdotH, roughness);
            float G_visibility = G * VdotH / (NdotL * VdotH);
            float Fc = pow(1.0 - VdotH, 5.0);
            AB += float2((1.0 - Fc) * G_visibility, Fc * G_visibility);
        }
    }

    AB /= (float) SAMPLE_COUNT;
    rhi::uni::tex_store(pc.lut, id.xy, AB);
}
