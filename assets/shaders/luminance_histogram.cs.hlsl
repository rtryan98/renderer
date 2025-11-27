// SHADER DEF compute_luminance_histogram
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "common/color/color_spaces.hlsli"
#include "shared/exposure_shared_types.h"

DECLARE_PUSH_CONSTANTS(Calculate_Luminance_Histogram_Push_Constants, pc);

groupshared uint histogram[256];

uint rec2020_to_bin(float3 color_rec2020, float min_log_luminance, float log_luminance_range)
{
    float luminance = ren::color::spaces::Rec2020_XYZ(color_rec2020).y;

    // don't count black pixels
    if (luminance < 0.001)
        return 0;

    float log2_luminance = log2(luminance);

    // don't count pixels beyond cutoff
    if (log2_luminance < pc.log_luminance_cutoff_low ||
        log2_luminance > pc.log_luminance_cutoff_high)
    {
        return 0;
    }

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
        float3 color_rec2020 = rhi::tex_load<float3>(pc.source_image, id).xyz;
        InterlockedAdd(histogram[rec2020_to_bin(color_rec2020, pc.min_log_luminance, pc.log_luminance_range)], 1);
    }

    GroupMemoryBarrierWithGroupSync();
    rhi::uni::atomic::buf_add<uint>(pc.luminance_histogram_buffer, idx, sizeof(float), histogram[idx]);
}
