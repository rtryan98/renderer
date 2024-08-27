#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/ocean_patch_types.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

const static float2 OFFSETS[6] = {
    float2(0.,0.),
    float2(1.,0.),
    float2(0.,1.),
    float2(1.,0.),
    float2(1.,1.),
    float2(0.,1.)
};

VS_Out main(uint vertex_id : SV_VertexID)
{
    rhi::Raw_Buffer camera_buffer = { pc.camera };
    GPU_Camera_Data camera = camera_buffer.load_nuri<GPU_Camera_Data>();

    uint quad_index = vertex_id / 6;
    uint triangle_vertex_index = vertex_id % 6;
    float2 vertex_pos = -pc.vertex_position_dist * float2(pc.field_size, pc.field_size) / 2.;
    vertex_pos += pc.vertex_position_dist * (float2(quad_index / pc.field_size, quad_index % pc.field_size) + OFFSETS[triangle_vertex_index]);

    float2 uvs[4] = {
        vertex_pos / pc.length_scales[0],
        vertex_pos / pc.length_scales[1],
        vertex_pos / pc.length_scales[2],
        vertex_pos / pc.length_scales[3]
    };
    SamplerState tex_sampler = rhi::Sampler(pc.tex_sampler).get_nuri();

    float4 x_y_z_xdx = float4(0.,0.,0.,0.);
    float4 ydx_zdx_ydy_zdy = float4(0.,0.,0.,0.);
    for (uint i = 0; i < 4; ++i)
    {
        x_y_z_xdx += rhi::Texture(pc.x_y_z_xdx_tex).sample_level_2d_array_uniform<float4>(tex_sampler, uvs[i], 0, 0);
        ydx_zdx_ydy_zdy += rhi::Texture(pc.ydx_zdx_ydy_zdy_tex).sample_level_2d_array_uniform<float4>(tex_sampler, uvs[i], 0, 0);
    }

    float3 displacement = float3(x_y_z_xdx.x, x_y_z_xdx.y, x_y_z_xdx.z);
    float4 pos = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    pos = mul(pos, camera.view_proj);
    VS_Out result = { pos, uvs };
    return result;
}
