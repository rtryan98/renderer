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

    float4 ocean_color = rhi::Texture(pc.ocean_color_tex).sample_2d_uniform<float4>(tex_sampler, ps_in.uv);
    float4 geom_color = rhi::Texture(pc.geom_color_tex).sample_2d_uniform<float4>(tex_sampler, ps_in.uv);
    float ocean_depth = rhi::Texture(pc.ocean_depth_tex).sample_2d_uniform<float>(tex_sampler, ps_in.uv);
    float geom_depth = rhi::Texture(pc.geom_depth_tex).sample_2d_uniform<float>(tex_sampler, ps_in.uv);

    float4 color;
    if (geom_depth < ocean_depth)
        color = geom_color;
    else
        color = ocean_color;

    PS_Out result = { color };
    return result;
}
