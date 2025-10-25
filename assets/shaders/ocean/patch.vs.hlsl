// SHADER DEF surface_patch_render_ocean
// ENTRYPOINT main
// TYPE vs
// DEFINE GROUP prepass
// DEFINE BOOL OCEAN_PATCH_PREPASS
// SHADER END DEF

#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shared/shared_resources.h"
#include "rhi/bindless.hlsli"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "ocean/ocean_render_utils.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

#if OCEAN_PATCH_PREPASS
struct Prepass_Out
{
    float4 position : SV_POSITION;
};
typedef Prepass_Out VS_OUT_TYPE;
#else
typedef VS_Out VS_OUT_TYPE;
#endif

VS_OUT_TYPE main(uint vertex_id : SV_VertexID)
{
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera);

    uint row = vertex_id / pc.vertices_per_axis;
    uint column = vertex_id % pc.vertices_per_axis;
    float2 vertex_pos = float2(pc.offset_x, pc.offset_y) - float2(pc.cell_size, pc.cell_size) / 2.;
    float vertex_delta = rcp(pc.vertices_per_axis - 1) * pc.cell_size;
    vertex_pos += vertex_delta * (float2(row, column));

    // vertex merging

    uint4 lod_differences = {
        (pc.lod_differences & 0xFF000000) >> 24,
        (pc.lod_differences & 0x00FF0000) >> 16,
        (pc.lod_differences & 0x0000FF00) >>  8,
        (pc.lod_differences & 0x000000FF) >>  0,
    };
    bool4 is_side = {
        row == pc.vertices_per_axis - 1,
        column == pc.vertices_per_axis - 1,
        row == 0,
        column == 0,
    };

    if (is_side[0] &&
        lod_differences[0] > 1)
    {
        uint shift = (lod_differences[0] - column) % lod_differences[0];
        vertex_pos.y += shift * vertex_delta;
    }
    if (is_side[1] &&
        lod_differences[1] > 1)
    {
        uint shift = (lod_differences[1] - row) % lod_differences[1];
        vertex_pos.x += shift * vertex_delta;
    }
    if (is_side[2] &&
        lod_differences[2] > 1)
    {
        uint shift = column % lod_differences[2];
        vertex_pos.y -= shift * vertex_delta;
    }
    if (is_side[3] &&
        lod_differences[3] > 1)
    {
        uint shift = row % lod_differences[3];
        vertex_pos.x -= shift * vertex_delta;
    }

    float2 uvs[4] = {
        vertex_pos / pc.length_scales[0],
        vertex_pos / pc.length_scales[1],
        vertex_pos / pc.length_scales[2],
        vertex_pos / pc.length_scales[3]
    };

    float3 displacement = 0.0;

    float4 weights = calculate_cascade_sampling_weights(
        distance(camera.position.xy, vertex_pos),
        0.25,
        5.0,
        pc.length_scales);
        
    Ocean_Min_Max_Values min_max_values = rhi::uni::buf_load<Ocean_Min_Max_Values>(pc.min_max_buffer);

    for (uint i = 0; i < 4; ++i)
    {
        if (weights[i] <= 0.0) continue;

        float4 x_y_z_xdx_ranges = min_max_values.cascades[i].max_values - min_max_values.cascades[i].min_values;

        float3 packed_displacement = rhi::uni::tex_sample_level_arr<float4>(pc.packed_displacement_tex, REN_SAMPLER_LINEAR_WRAP, uvs[i], i, 0.).xyz;
        packed_displacement *= x_y_z_xdx_ranges.xyz;
        packed_displacement += min_max_values.cascades[i].min_values.xyz;

        displacement += weights[i] * packed_displacement;
    }

    float4 pos_ws = float4(vertex_pos.x + displacement.x, vertex_pos.y + displacement.y, displacement.z, 1.);

    float4 pos = mul(camera.world_to_clip, pos_ws);
#if OCEAN_PATCH_PREPASS
    VS_OUT_TYPE result = { pos };
#else
    VS_OUT_TYPE result = { pos, uvs, pos_ws.xyz };
#endif
    return result;
}
