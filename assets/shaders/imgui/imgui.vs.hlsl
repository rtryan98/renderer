#include "shaders/imgui/imgui.hlsli"
#include "shaders/util.hlsli"

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

VS_Out main(uint vertex_index : SV_VertexID)
{
    VS_Out vs_out;
    Imgui_Vert vert = rhi::uni::buf_load_arr<Imgui_Vert>(pc.vertex_buffer_idx, vertex_index + pc.vertex_offset);
    vs_out.pos = mul(float4(vert.pos, 0.0, 1.0), ortho(pc.left, pc.top, pc.right, pc.bottom));
    vs_out.col = ren::unpack_unorm_4x8(vert.col);
    vs_out.uv = vert.uv;
    return vs_out;
}
