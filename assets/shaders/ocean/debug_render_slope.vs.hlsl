#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/debug_render_types.hlsli"
#include "ocean/ocean_render_utils.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Debug_Push_Constants, pc);

VS_Out main(uint vertex_id : SV_VertexID)
{
    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera);

    uint vertex_index = vertex_id / 2;
    float2 origin = -pc.point_dist * float2(pc.point_field_size, pc.point_field_size) / 2.;
    float2 vertex_pos = origin + pc.point_dist * float2(vertex_index / pc.point_field_size, vertex_index % pc.point_field_size);
    float2 uvs[4] = {
        vertex_pos / pc.length_scales[0],
        vertex_pos / pc.length_scales[1],
        vertex_pos / pc.length_scales[2],
        vertex_pos / pc.length_scales[3]
    };

    float4 x_y_z_xdx = float4(0.,0.,0.,0.);
    float4 ydx_zdx_ydy_zdy = float4(0.,0.,0.,0.);
    for (uint i = 0; i < 4; ++i)
    {
        x_y_z_xdx += rhi::uni::tex_sample_level_arr<float4>(pc.x_y_z_xdx_tex, pc.tex_sampler, uvs[i], i, 0.);
        ydx_zdx_ydy_zdy += rhi::uni::tex_sample_level_arr<float4>(pc.ydx_zdx_ydy_zdy_tex, pc.tex_sampler, uvs[i], i, 0.);
    }

    float2 slope = calculate_slope(
        ydx_zdx_ydy_zdy[1], ydx_zdx_ydy_zdy[3], x_y_z_xdx[3], ydx_zdx_ydy_zdy[2]
    );
    float line_length = float(vertex_id % 2) * pc.line_scale;

    float3 displacement = float3(x_y_z_xdx.x, x_y_z_xdx.y, x_y_z_xdx.z) + line_length * float3(-slope.x, -slope.y, 0.);
    float4 pos = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    pos = mul(pos, camera.view_proj);
    float4 col = float4(0.5,1.,1.,1.);
    VS_Out result = { pos, col };
    return result;
}
