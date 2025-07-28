#include "shared/camera_shared_types.h"
#include "shared/draw_shared_types.h"
#include "rhi/bindless.hlsli"
#include "draw/basic_draw.hlsli"

DECLARE_PUSH_CONSTANTS(Immediate_Draw_Push_Constants, pc);

struct Vertex_Attribute_Data
{
    float3 normal;
    float4 tangent;
    float2 tex_coord;
    uint color;
};

VS_Out main(uint vertex_id : SV_VertexID, uint instance_index : SV_StartInstanceLocation)
{
    rhi::Array_Buffer vertex_position_buffer = { pc.position_buffer };
    rhi::Array_Buffer vertex_attribute_buffer = { pc.attribute_buffer };
    rhi::Array_Buffer instance_transform_buffer = { pc.instance_transform_buffer };
    rhi::Raw_Buffer camera_buffer = { pc.camera_buffer };
    uint vertex_index = vertex_id + pc.vertex_offset;

    GPU_Camera_Data camera = camera_buffer.load_nuri<GPU_Camera_Data>();
    GPU_Instance instance_transform = instance_transform_buffer.load_nuri<GPU_Instance>(instance_index);

    float4 vertex_pos = float4(vertex_position_buffer.load_nuri<float3>(vertex_index), 1.0);
    float4x4 mesh_to_world = float4x4(
        instance_transform.mesh_to_world[0],
        instance_transform.mesh_to_world[1],
        instance_transform.mesh_to_world[2],
        float4(0.0, 0.0, 0.0, 1.0));
    vertex_pos = mul(vertex_pos, mesh_to_world);
    Vertex_Attribute_Data vertex_attributes = vertex_attribute_buffer.load_nuri<Vertex_Attribute_Data>(vertex_index);
    vertex_attributes.normal = normalize(mul(vertex_attributes.normal, instance_transform.normal_to_world));

    VS_Out result = {
        mul(float4(vertex_pos.x, vertex_pos.y, vertex_pos.z, 1.0), camera.view_proj),
        vertex_attributes.normal,
        vertex_attributes.tangent,
        vertex_attributes.tex_coord
    };
    return result;
}
