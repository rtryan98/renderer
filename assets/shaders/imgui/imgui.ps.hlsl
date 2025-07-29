#include "shaders/imgui/imgui.hlsli"

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

PS_Out main(PS_In ps_in)
{
    PS_Out ps_out;
    ps_out.col = ps_in.col * rhi::tex_sample<float4>(pc.texture_idx, pc.sampler_idx, ps_in.uv);
    return ps_out;
}
