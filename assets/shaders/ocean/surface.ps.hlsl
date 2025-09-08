#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
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
    float4 x_y_z_xdx = float4(0.,0.,0.,0.);
    float4 ydx_zdx_ydy_zdy = float4(0.,0.,0.,0.);
    for (uint i = 0; i < 4; ++i)
    {
        x_y_z_xdx += rhi::uni::tex_sample_arr<float4>(pc.x_y_z_xdx_tex, pc.tex_sampler, ps_in.uvs[i], i);
        ydx_zdx_ydy_zdy += rhi::uni::tex_sample_arr<float4>(pc.ydx_zdx_ydy_zdy_tex, pc.tex_sampler, ps_in.uvs[i], i);
    }

    float x_dx = x_y_z_xdx[3];
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

    float3 V = normalize(float3(ps_in.position_camera.xyz - ps_in.position_ws.xyz));

    float3 color = ren::pbr::evaluate_lights(V, surface);

    return float4(color, 1.0);
}
