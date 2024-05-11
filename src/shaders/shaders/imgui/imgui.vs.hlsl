#include "renderer/imgui/imgui.hlsli"
#include "renderer/util.hlsli"

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

VS_Out main(uint vertex_index : SV_VertexID)
{
    rhi::Array_Buffer vertex_buffer = {pc.vertex_buffer_idx};
    
    VS_Out vs_out;
    Imgui_Vert vert = vertex_buffer.load<Imgui_Vert>(vertex_index + pc.vertex_offset);
    vs_out.pos = mul(float4(vert.pos, 0.0, 1.0), ortho(pc.left, pc.top, pc.right, pc.bottom));
    vs_out.col = ren::unpack_unorm_4x8(vert.col);
    vs_out.uv = vert.uv;
    return vs_out;
}
