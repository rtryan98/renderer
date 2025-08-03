#include "rhi/bindless.hlsli"
#include "shared/tonemap_shared_types.h"

DECLARE_PUSH_CONSTANTS(Tonemap_Push_Constants, pc);

struct PS_In {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PS_Out {
    float4 color : SV_Target;
};

PS_Out main(PS_In ps_in)
{
    float4 source_color = rhi::uni::tex_sample<float4>(pc.source_texture, pc.texture_sampler, ps_in.uv);

    PS_Out result = { source_color };
    return result;
}
