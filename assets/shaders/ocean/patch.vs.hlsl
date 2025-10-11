#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shared/shared_resources.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "ocean/ocean_render_utils.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

VS_Out main(uint vertex_id : SV_VertexID)
{
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera);

    uint row = vertex_id / pc.field_size;
    uint column = vertex_id % pc.field_size;
    float2 vertex_pos = -pc.vertex_position_dist * float2(pc.field_size, pc.field_size) / 2.;
    vertex_pos += pc.vertex_position_dist * (float2(row, column));
    vertex_pos += float2(pc.offset_x, pc.offset_y);

    float2 uvs[4] = {
        vertex_pos / pc.length_scales[0],
        vertex_pos / pc.length_scales[1],
        vertex_pos / pc.length_scales[2],
        vertex_pos / pc.length_scales[3]
    };

    float4 x_y_z_xdx = float4(0.,0.,0.,0.);

    float4 weights = calculate_cascade_sampling_weights(
        distance(camera.position.xy, vertex_pos),
        0.25,
        5.0,
        pc.length_scales);

    for (uint i = 0; i < 4; ++i)
    {
        if (weights[i] <= 0.0) continue;

        x_y_z_xdx += weights[i] * rhi::uni::tex_sample_level_arr<float4>(pc.x_y_z_xdx_tex, REN_SAMPLER_LINEAR_WRAP, uvs[i], i, 0.);
    }

    float3 displacement = float3(x_y_z_xdx.x, x_y_z_xdx.y, x_y_z_xdx.z);
    float4 pos_ws = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    float4 pos = mul(camera.world_to_clip, pos_ws);
    VS_Out result = { pos, uvs, pos_ws, camera.position };
    return result;
}
