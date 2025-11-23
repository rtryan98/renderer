// SHADER DEF apply_exposure
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/exposure_shared_types.h"
#include "rhi/bindless.hlsli"

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

float sensor_saturation(float ev100)
{
    return EXPOSURE_SATURATION_FACTOR * pow(2.0, ev100);
}

float exposure_from_ev100(float ev100)
{
    return rcp(sensor_saturation(ev100));
}

float exposure_from_camera(float aperture, float shutter, float iso)
{
    return exposure_from_ev100(ev100_from_camera(aperture, shutter, iso));
}

float exposure_from_luminance(float average_luminance)
{
    return exposure_from_ev100(ev100_from_luminance(average_luminance));
}

[shader("compute")]
[numthreads(16,16,1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= pc.image_size.x || id.y >= pc.image_size.y)
        return;

    float4 color = rhi::tex_load<float4>(pc.image, id);
    float exposure = 0.;

    if (pc.use_camera_exposure > 0)
    {
        exposure = exposure_from_camera(pc.aperture, pc.shutter, pc.iso);
    }
    else
    {
        float average_luminance = rhi::buf_load<Luminance_Histogram>(pc.luminance_histogram_buffer).average_luminance;
        exposure = exposure_from_luminance(average_luminance);
    }

    rhi::tex_store(pc.image, id, color * exposure);
}
