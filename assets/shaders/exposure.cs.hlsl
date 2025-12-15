// SHADER DEF apply_exposure
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/exposure_shared_types.h"
#include "rhi/bindless.hlsli"
#include "common/pbr/light_units.hlsli"

DECLARE_PUSH_CONSTANTS(Apply_Exposure_Push_Constants, pc);

static const float EXPOSURE_CALIBRATION_CONSTANT = 12.5;
static const float EXPOSURE_SENSOR_SENSITIVITY = 100.;
static const float EXPOSURE_SATURATION_FACTOR = 1.2; // 78.0 / (0.65 * EXPOSURE_SENSOR_SENSITIVITY);

float ev100_from_camera(float aperture, float shutter, float iso)
{
    return log2((aperture * aperture) / shutter * 100. / iso);
}

float ev100_from_luminance(float average_luminance)
{
    return log2(average_luminance * EXPOSURE_SENSOR_SENSITIVITY / EXPOSURE_CALIBRATION_CONSTANT);
}

float exposure_from_ev100(float ev100, float exposure_compensation_ev = 0.)
{
    float max_luminance = EXPOSURE_SATURATION_FACTOR * pow(2.0, ev100 - exposure_compensation_ev);
    return 1.0 / max_luminance;
}

float exposure_from_camera(float aperture, float shutter, float iso)
{
    return exposure_from_ev100(ev100_from_camera(aperture, shutter, iso));
}

float exposure_from_luminance(float average_luminance, float exposure_compensation_ev = 0.)
{
    return exposure_from_ev100(ev100_from_luminance(average_luminance), exposure_compensation_ev);
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= pc.image_size.x || id.y >= pc.image_size.y)
        return;

    // Color is pre-exposed. Revert pre-exposure first.
    float4 color = ren::framebuffer_referred_to_luminance(rhi::tex_load<float4>(pc.image, id));
    float exposure = 0.;

    if (pc.use_camera_exposure > 0)
    {
        exposure = exposure_from_camera(pc.aperture, pc.shutter, pc.iso);
    }
    else
    {
        float average_luminance = rhi::buf_load<Luminance_Histogram>(pc.luminance_histogram_buffer).average_luminance;
        exposure = exposure_from_luminance(average_luminance, pc.auto_exposure_compensation);
    }

    // Apply pre-exposure again to maintain precision.
    rhi::tex_store(pc.image, id, ren::luminance_to_framebuffer_referred(color * exposure));
}
