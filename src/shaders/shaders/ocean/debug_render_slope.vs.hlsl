#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/debug_render_types.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Debug_Push_Constants, pc);

VS_Out main(uint vertex_id : SV_VertexID)
{
    rhi::Array_Buffer vertices = { pc.vertices };
    float2 vertex = vertices.load<float2>(vertex_id / 2);
    float dir_factor = float(vertex_id % 2);

    float4 pos;
    float4 col;
    VS_Out result = { pos, col };
}
