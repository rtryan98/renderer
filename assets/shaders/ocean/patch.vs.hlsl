#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "ocean/ocean_render_utils.hlsli"

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
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera);

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

    float4 x_y_z_xdx = float4(0.,0.,0.,0.);
    float4 ydx_zdx_ydy_zdy = float4(0.,0.,0.,0.);

    for (uint i = 0; i < 4; ++i)
    {
        x_y_z_xdx += rhi::uni::tex_sample_level_arr<float4>(pc.x_y_z_xdx_tex, pc.tex_sampler, uvs[i], i, 0.);
        ydx_zdx_ydy_zdy += rhi::uni::tex_sample_level_arr<float4>(pc.ydx_zdx_ydy_zdy_tex, pc.tex_sampler, uvs[i], i, 0.);
    }

    float x_dx = x_y_z_xdx.w;
    float y_dx = ydx_zdx_ydy_zdy.x;
    float z_dx = ydx_zdx_ydy_zdy.y;
    float y_dy = ydx_zdx_ydy_zdy.z;
    float z_dy = ydx_zdx_ydy_zdy.w;

    float3 displacement = float3(x_y_z_xdx.x, x_y_z_xdx.y, x_y_z_xdx.z);
    // float3 normal = calculate_normals(calculate_slope(z_dx, z_dy, x_dx, y_dy));
    // float jacobian = calculate_det_jacobian(x_dx, y_dx, y_dx, y_dy);
    // float2 loop_remove = heaviside(-jacobian) * normal.xy * jacobian;
    // displacement.xy += loop_remove;
    float4 pos_ws = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    float4 pos = mul(pos_ws, camera.view_proj);
    VS_Out result = { pos, uvs, pos_ws, camera.position };
    return result;
}
