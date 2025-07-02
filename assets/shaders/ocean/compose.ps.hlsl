#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Composition_Push_Constants, pc);

struct PS_In {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PS_Out {
    float4 color : SV_Target;
};

PS_Out main(PS_In ps_in)
{
    SamplerState tex_sampler = rhi::Sampler(pc.tex_sampler).get_nuri();
    PS_Out result = { rhi::Texture(pc.rt_color_tex).sample_2d_uniform<float4>(tex_sampler, ps_in.uv) };
    return result;
}
