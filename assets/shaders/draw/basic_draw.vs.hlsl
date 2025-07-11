#include "shared/camera_shared_types.h"
#include "shared/draw_shared_types.h"
#include "rhi/bindless.hlsli"
#include "draw/basic_draw.hlsli"

DECLARE_PUSH_CONSTANTS(Immediate_Draw_Push_Constants, pc);

struct Vertex_Attribute_Data
{
    float3 normal;
    float3 tangent;
    float2 tex_coord;
    uint color;
};

VS_Out main(uint vertex_id : SV_VertexID)
{
    rhi::Array_Buffer vertex_position_buffer = { pc.position_buffer };
    rhi::Array_Buffer vertex_attribute_buffer = { pc.attribute_buffer };
    rhi::Raw_Buffer camera_buffer = { pc.camera_buffer };
    uint vertex_index = vertex_id + pc.vertex_offset;

    GPU_Camera_Data camera = camera_buffer.load_nuri<GPU_Camera_Data>();
    float3 vertex_pos = vertex_position_buffer.load_nuri<float3>(vertex_index);
    vertex_pos.z -= .5;
    Vertex_Attribute_Data vertex_attributes = vertex_attribute_buffer.load_nuri<Vertex_Attribute_Data>(vertex_index);

    VS_Out result = {
        mul(float4(vertex_pos.x, vertex_pos.y, vertex_pos.z, 1.0), camera.view_proj),
        vertex_attributes.normal,
        vertex_attributes.tangent,
        vertex_attributes.tex_coord
    };
    return result;
}
