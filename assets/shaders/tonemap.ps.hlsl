#include "rhi/bindless.hlsli"
#include "shared/tonemap_shared_types.h"
#include "common/color/transfer_functions.hlsli"

DECLARE_PUSH_CONSTANTS(Tonemap_Push_Constants, pc);

struct PS_In {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PS_In ps_in) : SV_Target
{
    float4 color = rhi::uni::tex_sample<float4>(pc.source_texture, pc.texture_sampler, ps_in.uv);
    color.xyz = ren::color::transfer_functions::IEOTF_sRGB(color.xyz);
    return color;
}
