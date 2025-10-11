#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shared/shared_resources.h"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "rhi/bindless.hlsli"
#include "constants.hlsli"
#include "ocean/ocean_render_utils.hlsli"
#include "common/pbr/pbr.hlsli"
#include "common/color/color_spaces.hlsli"

#include "common/pbr/lighting.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

float4 main(PS_In ps_in) : SV_Target
{
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera);
    Ocean_Min_Max_Values min_max_values = rhi::uni::buf_load<Ocean_Min_Max_Values>(pc.min_max_buffer);

    float x_dx = 0.0;
    float4 ydx_zdx_ydy_zdy = 0.0;

    float4 weights = calculate_cascade_sampling_weights(
        distance(camera.position.xy, ps_in.position_ws.xy),
        0.25,
        5.0,
        pc.length_scales);

    for (uint i = 0; i < 4; ++i)
    {
        if (weights[i] <= 0.0) continue;

        float4 x_y_z_xdx_ranges = min_max_values.cascades[i].max_values - min_max_values.cascades[i].min_values;
        float4 ydx_zdx_ydy_zdy_ranges = min_max_values.cascades[i + 4].max_values - min_max_values.cascades[i + 4].min_values;
        
        float packed_xdx = rhi::uni::tex_sample_level_arr<float>(pc.packed_xdx_tex, REN_SAMPLER_LINEAR_WRAP, ps_in.uvs[i], i, 0.);
        packed_xdx *= x_y_z_xdx_ranges.w;
        packed_xdx += min_max_values.cascades[i].min_values.w;

        x_dx += weights[i] * packed_xdx;

        float4 packed_derivatives = rhi::uni::tex_sample_level_arr<float4>(pc.packed_derivatives_tex, REN_SAMPLER_LINEAR_WRAP, ps_in.uvs[i], i, 0.);
        packed_derivatives *= ydx_zdx_ydy_zdy_ranges;
        packed_derivatives += min_max_values.cascades[i + 4].min_values;
        ydx_zdx_ydy_zdy += weights[i] * packed_derivatives;
    }

    float y_dx = ydx_zdx_ydy_zdy[0];
    float z_dx = ydx_zdx_ydy_zdy[1];
    float y_dy = ydx_zdx_ydy_zdy[2];
    float z_dy = ydx_zdx_ydy_zdy[3];

    float jacobian = calculate_det_jacobian(x_dx, y_dx, y_dx, y_dy);

    float3 diffuse_color = float3(0.11952194901, 0.24474843921, 0.42706129019);
    ren::pbr::Surface surface;
    surface.position = ps_in.position_ws.xyz;
    surface.albedo = ren::color::spaces::Rec709_Rec2020(diffuse_color);
    surface.normal = calculate_normals(calculate_slope(z_dx, z_dy, x_dx, y_dy));
    surface.metallic = 0.0625;
    surface.roughness = 0.0125;
    if (jacobian < 0.7)
    {
        surface.albedo = 1.0;
        surface.roughness = 1.0;
        surface.metallic = 0.0;
    }

    float3 V = normalize(float3(camera.position.xyz - ps_in.position_ws.xyz));

    float3 color = ren::pbr::evaluate_lights(V, surface);

    return float4(color, 1.0);
}
