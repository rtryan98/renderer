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

VS_Out main(uint vertex_id : SV_VertexID, uint vertex_offset : SV_StartVertexLocation, uint instance_index : SV_StartInstanceLocation)
{
    uint vertex_index = vertex_id + vertex_offset;

    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    GPU_Instance instance_transform = rhi::uni::buf_load_arr<GPU_Instance>(pc.instance_transform_buffer, instance_index);

    float4 vertex_pos = float4(rhi::uni::buf_load_arr<float3>(pc.position_buffer, vertex_index), 1.0);
    vertex_pos = mul(camera.view_proj, mul(instance_transform.mesh_to_world, vertex_pos));
    Vertex_Attribute_Data vertex_attributes = rhi::uni::buf_load_arr<Vertex_Attribute_Data>(pc.attribute_buffer, vertex_index);
    vertex_attributes.normal = normalize(mul(vertex_attributes.normal, instance_transform.normal_to_world));

    VS_Out result = {
        vertex_pos,
        vertex_attributes.normal,
        vertex_attributes.tangent,
        vertex_attributes.tex_coord,
        instance_index
    };
    return result;
}
