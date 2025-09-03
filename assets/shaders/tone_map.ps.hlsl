#include "rhi/bindless.hlsli"
#include "shared/tone_map_shared_types.h"
#include "common/color/transfer_functions.hlsli"
#include "common/color/color_spaces.hlsli"

DECLARE_PUSH_CONSTANTS(Tone_Map_Push_Constants, pc);

struct PS_In {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 evaluate_gt7_curve(float3 input, GT7_Tone_Mapping_Data tone_map_parameters)
{
    bool3 valid = input >= 0.0;
    float3 weight_linear = smoothstep(0.0, tone_map_parameters.mid_point, input);
    float3 weight_toe = 1.0 - weight_linear;
    float3 shoulder = tone_map_parameters.kA + tone_map_parameters.kB * exp(input * tone_map_parameters.kC);
    float3 toe = tone_map_parameters.mid_point * pow(input / tone_map_parameters.mid_point, tone_map_parameters.toe_strength);
    float3 toe_linear = weight_toe * toe + weight_linear * input;
    return valid * select(input < tone_map_parameters.linear_section * tone_map_parameters.luminance_target, toe_linear, shoulder);
}

template<typename T>
T chroma_curve(T a, T b, float x)
{
    return 1.0 - smoothstep(a, b, x);
}

float3 apply_gt7_tone_map(float3 input, GT7_Tone_Mapping_Data tone_map_parameters)
{
    float3 ucs = ren::color::spaces::Rec2020_Jzazbz(input, tone_map_parameters.reference_luminance);
    float3 skewed_rgb = evaluate_gt7_curve(input, tone_map_parameters);
    float3 skewed_ucs = ren::color::spaces::Rec2020_Jzazbz(skewed_rgb, tone_map_parameters.reference_luminance);
    float chroma_scale = chroma_curve(tone_map_parameters.fade_start, tone_map_parameters.fade_end, ucs.x / tone_map_parameters.luminance_target_Jzazbz);
    float3 scaled_ucs = float3(skewed_ucs.x, chroma_scale * ucs.y, chroma_scale * ucs.z);
    float3 scaled_rgb = ren::color::spaces::Jzazbz_Rec2020(scaled_ucs, tone_map_parameters.reference_luminance);
    float3 blended = (1.0 - tone_map_parameters.blend_ratio) * skewed_rgb + tone_map_parameters.blend_ratio * scaled_rgb;
    return tone_map_parameters.sdr_correction_factor * min(blended, tone_map_parameters.luminance_target);
}

float4 main(PS_In ps_in) : SV_Target
{
    GT7_Tone_Mapping_Data tone_map_parameters = rhi::uni::buf_load<GT7_Tone_Mapping_Data>(pc.tone_map_parameters_buffer);
    float4 color = max(rhi::uni::tex_sample<float4>(pc.source_texture, pc.texture_sampler, ps_in.uv), 0.0);
    color.xyz = apply_gt7_tone_map(color.xyz, tone_map_parameters);

    if (tone_map_parameters.is_hdr != 0)
    {
        color.xyz = ren::color::transfer_functions::IEOTF_PQ(color.xyz * tone_map_parameters.reference_luminance);
    }
    else
    {
        // Colors are in Rec.2020 so we need to convert them back to Rec.709.
        // Clipping should be acceptable so long as the image was properly exposed, as the tone mapper does a color volume transform.
        // TODO: if clipping looks bad, implement gamut compression.
        color.xyz = saturate(ren::color::spaces::Rec2020_Rec709(color.xyz));
        color.xyz = ren::color::transfer_functions::IEOTF_sRGB(color.xyz);
    }

    return color;
}
