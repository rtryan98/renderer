// SHADER DEF compute_luminance_histogram
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "common/color/color_spaces.hlsli"
#include "shared/exposure_shared_types.h"
#include "common/pbr/light_units.hlsli"

DECLARE_PUSH_CONSTANTS(Calculate_Luminance_Histogram_Push_Constants, pc);

groupshared uint histogram[256];

uint luminance_to_bin(float luminance, float min_log_luminance, float log_luminance_range)
{
    // don't count pixels under minimum
    if (luminance < ren::EV_to_luminance(min_log_luminance))
        return 0;

    float log2_luminance = log2(luminance);

    log2_luminance = saturate(rcp(log_luminance_range) * (log2_luminance - min_log_luminance));

    return (uint) (log2_luminance * 254. + 1.);
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint2 id : SV_DispatchThreadID, uint idx : SV_GroupIndex)
{
    histogram[idx] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (id.x < pc.image_width && id.y < pc.image_height)
    {
        // The luminance is calculated from a pre-exposed Rec.2020 color.
        // Thus, the pre-exposure must be reverted before the color is transformed to CIE XYZ.
        float3 color_rec2020 = ren::framebuffer_referred_to_luminance(rhi::tex_load<float3>(pc.source_image, id).xyz);
        float luminance = ren::color::spaces::Rec2020_XYZ(color_rec2020).y;
        InterlockedAdd(histogram[luminance_to_bin(
                luminance,
                pc.min_log_luminance,
                pc.log_luminance_range)],
            1);
    }

    GroupMemoryBarrierWithGroupSync();
    rhi::uni::atomic::buf_add<uint>(pc.luminance_histogram_buffer, idx, sizeof(float), histogram[idx]);
}
