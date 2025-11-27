// SHADER DEF compute_luminance_histogram_average
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "common/color/color_spaces.hlsli"
#include "shared/exposure_shared_types.h"

DECLARE_PUSH_CONSTANTS(Calculate_Average_Luminance_Push_Constants, pc);

#define BUCKET_COUNT 256

groupshared float histogram[BUCKET_COUNT];

[shader("compute")]
[numthreads(BUCKET_COUNT,1,1)]
void main(uint idx : SV_GroupIndex)
{
    uint bin_count = rhi::uni::buf_load<Luminance_Histogram>(pc.luminance_histogram_buffer).buckets[idx];
    histogram[idx] = bin_count * float(idx);

    GroupMemoryBarrierWithGroupSync();

    rhi::uni::buf_store_arr(pc.luminance_histogram_buffer, 1 + idx, 0);

    for (uint thread_cutoff = BUCKET_COUNT >> 1; thread_cutoff > 0; thread_cutoff >>= 1)
    {
        if (idx < thread_cutoff)
        {
            histogram[idx] += histogram[idx + thread_cutoff];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (idx == 0)
    {
        float pixel_count = (float) pc.pixel_count - bin_count;
        float weighted_log2_average_luminance = (histogram[0] / max(pixel_count, 1.)) - 1.;
        float weighted_average_luminance = exp2(weighted_log2_average_luminance / 254. * pc.log_luminance_range + pc.min_log_luminance);
        float last_frame_luminance = rhi::buf_load<float>(pc.luminance_histogram_buffer);
        float current_luminance = last_frame_luminance + (weighted_average_luminance - last_frame_luminance) * (1. - exp(-pc.delta_time * pc.tau));
        rhi::buf_store<float>(pc.luminance_histogram_buffer, current_luminance);
    }
}
