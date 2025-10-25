// SHADER DEF ocean_texture_reorder
// ENTRYPOINT main
// TYPE cs
// SHADER END DEF

#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Reorder_Push_Constants, pc);

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    Ocean_Min_Max_Values min_max_values = rhi::uni::buf_load<Ocean_Min_Max_Values>(pc.min_max_buffer);
    float4 x_y_z_xdx_ranges = min_max_values.cascades[id.z].max_values - min_max_values.cascades[id.z].min_values;
    float4 ydx_zdx_ydy_zdy_ranges = min_max_values.cascades[id.z + 4].max_values - min_max_values.cascades[id.z + 4].min_values;

    // Move into 0.0 - 1.0 range
    float4 x_y_z_xdx = rhi::uni::tex_load_arr<float4>(pc.x_y_z_xdx_tex, id.xy, id.z);
    x_y_z_xdx -= min_max_values.cascades[id.z].min_values;
    x_y_z_xdx /= max(x_y_z_xdx_ranges, 1.0/1024.0);

    float4 ydx_zdx_ydy_zdy = rhi::uni::tex_load_arr<float4>(pc.ydx_zdx_ydy_zdy_tex, id.xy, id.z);
    ydx_zdx_ydy_zdy -= min_max_values.cascades[id.z + 4].min_values;
    ydx_zdx_ydy_zdy /= max(ydx_zdx_ydy_zdy_ranges, 1.0/1024.0);

    // Compress
    rhi::uni::tex_store_arr(pc.displacement_tex, id.xy, id.z, float4(x_y_z_xdx.xyz, 0.0)); // R10G10B10A2_UNORM
    rhi::uni::tex_store_arr(        pc.foam_tex, id.xy, id.z,                x_y_z_xdx.w); // R8_UNORM
    rhi::uni::tex_store_arr( pc.derivatives_tex, id.xy, id.z,            ydx_zdx_ydy_zdy); // R8G8B8A8_UNORM
}
