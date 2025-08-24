#include "rhi/bindless.hlsli"
#include "shared/tone_map_shared_types.h"

DECLARE_PUSH_CONSTANTS(Tone_Map_Debug_Quads_Push_Constants, pc);

struct PS_In {
    float4 position : SV_Position;
    float hue: COLOR0;
    float value : COLOR1;
};

float4 main(PS_In ps_in) : SV_Target
{
    float HUE_SPLITTER = 120.0;
    float value = pow(10.0, ps_in.value);
    float hue = ps_in.hue;
    hue = floor(HUE_SPLITTER * hue) / HUE_SPLITTER;
    float3 rgb;
    rgb.r = fmod(5.0 + hue * 6.0, 6.0);
    rgb.r = 1.0 - max(min(rgb.r, min(4.0-rgb.r, 1.0)), 0.0);
    rgb.g = fmod(3.0 + hue * 6.0, 6.0);
    rgb.g = 1.0 - max(min(rgb.g, min(4.0-rgb.g, 1.0)), 0.0);
    rgb.b = fmod(1.0 + hue * 6.0, 6.0);
    rgb.b = 1.0 - max(min(rgb.b, min(4.0-rgb.b, 1.0)), 0.0);
    return float4(value * rgb, 1.0);
}
