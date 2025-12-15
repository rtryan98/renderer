#ifndef REN_LIGHT_UNITS
#define REN_LIGHT_UNITS

#include "constants.hlsli"

namespace ren
{
// Luminous Energy Qv in Lumen-seconds (lm*s)
// Luminous power in Lumen (lm)
// Luminous intensity in Candela (cd)
// Illuminance in Lux (lx)
// Luminance in Nits (cd/m2)

static const float FRAMEBUFFER_REFERENCE_COMPRESSION_VALUE = 100.; // Pre-Exposure
static const float EXPOSURE_S = 100.; // ISO-arithmetic
static const float EXPOSURE_K = 12.5; // Calibration constant

// Textures with fp16 formats or lower precision cannot hold the high values from direct sun illumination.
// According to Lagarde and Rousiers, the dynamic range of small float formats is roughly 30 f-stops.
// To prevent NaNs and infs, "pre-exposing" the image is required.
// This works well because the lower values are more likely to be under-utilized in the first place.
// Apply at the end of the lighting pipeline and reverse where required.
template<typename T>
T luminance_to_framebuffer_referred(T value)
{
    return value / FRAMEBUFFER_REFERENCE_COMPRESSION_VALUE;
}

template<typename T>
T framebuffer_referred_to_luminance(T value)
{
    return value * FRAMEBUFFER_REFERENCE_COMPRESSION_VALUE;
}

// EV
float luminance_to_EV(float luminance)
{
    return log2(luminance * EXPOSURE_S / EXPOSURE_K);
}

float EV_to_luminance(float EV)
{
    return exp2(EV) * EXPOSURE_K / EXPOSURE_S;
}

// Punctual light attenuation (Illuminance)
float attenuation_point_light(float distance2)
{
    float attenuation = rcp(max(4. * PI * distance2, 0.01 * 0.01));
    return attenuation;
}

} // namespace ren

#endif
