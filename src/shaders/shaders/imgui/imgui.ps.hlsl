#include "shaders/imgui/imgui.hlsli"

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

PS_Out main(PS_In ps_in)
{
    rhi::Texture texture = {pc.texture_idx};
    rhi::Sampler sampler = {pc.sampler_idx};

    PS_Out ps_out;
    ps_out.col = ps_in.col * texture.sample_2d<float4>(sampler.get(), ps_in.uv);
    return ps_out;
}
