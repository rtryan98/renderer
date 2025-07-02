#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/debug_render_types.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Debug_Push_Constants, pc);

float2 calculate_slope(float z_dx, float z_dy, float x_dx, float y_dy)
{
    return float2(z_dx / 1.0 + /* lambda * */x_dx, z_dy / 1.0 + /* lambda * */y_dy);
}

VS_Out main(uint vertex_id : SV_VertexID)
{
    rhi::Raw_Buffer camera_buffer = { pc.camera };
    GPU_Camera_Data camera = camera_buffer.load_nuri<GPU_Camera_Data>();

    uint vertex_index = vertex_id / 2;
    float2 origin = -pc.point_dist * float2(pc.point_field_size, pc.point_field_size) / 2.;
    float2 vertex_pos = origin + pc.point_dist * float2(vertex_index / pc.point_field_size, vertex_index % pc.point_field_size);
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
        x_y_z_xdx += rhi::Texture(pc.x_y_z_xdx_tex).sample_level_2d_array_uniform<float4>(tex_sampler, uvs[i], 0, i);
        ydx_zdx_ydy_zdy += rhi::Texture(pc.ydx_zdx_ydy_zdy_tex).sample_level_2d_array_uniform<float4>(tex_sampler, uvs[i], 0, i);
    }

    float2 slope = calculate_slope(
        ydx_zdx_ydy_zdy[1], ydx_zdx_ydy_zdy[3], x_y_z_xdx[3], ydx_zdx_ydy_zdy[2]
    );
    float line_length = float(vertex_id % 2) * pc.line_scale;

    float3 displacement = float3(x_y_z_xdx.x, x_y_z_xdx.y, x_y_z_xdx.z) + line_length * float3(slope.x, slope.y, 0.);
    float4 pos = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    pos = mul(pos, camera.view_proj);
    float4 col = float4(0.5,1.,1.,1.);
    VS_Out result = { pos, col };
    return result;
}
