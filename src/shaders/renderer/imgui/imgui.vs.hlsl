#include "renderer/imgui/imgui.hlsli"
#include "renderer/util.hlsli"

// compile from renderer/thirdparty/rhi/thirdparty/directx_shader_compiler/bin/x64
// dxc -enable-16bit-types -HV 2021 -no-legacy-cbuf-layout -Zpr -O3 -I ../../../../../../src/shaders -I ../../../../../../thirdparty/rhi/src/shaders -E "main" -T vs_6_6 -Fo ../../../../../../src/renderer/renderer/generated/imgui.vs.dxil ../../../../../../src/shaders/renderer/imgui/imgui.vs.hlsl
// dxc -enable-16bit-types -HV 2021 -no-legacy-cbuf-layout -Zpr -O3 -spirv -fvk-invert-y -fvk-use-dx-position-w -fvk-use-scalar-layout -fspv-use-legacy-buffer-matrix-order -I ../../../../../../src/shaders -I ../../../../../../thirdparty/rhi/src/shaders -E "main" -T vs_6_6 -Fo ../../../../../../src/renderer/renderer/generated/imgui.vs.spv ../../../../../../src/shaders/renderer/imgui/imgui.vs.hlsl

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

VS_Out main(uint vertex_index : SV_VertexID)
{
    VS_Out vs_out;
    Imgui_Vert vert = pc.vertex_buffer.load<Imgui_Vert>(vertex_index);
    vs_out.pos = mul(ortho(pc.left, pc.top, pc.right, pc.bottom), float4(vert.pos, 0.0, 1.0));
    vs_out.col = ren::unpack_unorm_4x8(vert.col);
    vs_out.uv = vert.uv;
    return vs_out;
}
