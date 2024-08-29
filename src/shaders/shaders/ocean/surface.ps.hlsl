#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

float2 calculate_slope(float z_dx, float z_dy, float x_dx, float y_dy)
{
    return float2(z_dx / 1.0 + /* lambda * */x_dx, z_dy / 1.0 + /* lambda * */y_dy);
}

float3 calculate_normals(float2 slope)
{
    return normalize(float3(-slope.x, -slope.y, 1.0));
}

PS_Out main(PS_In ps_in)
{
    SamplerState tex_sampler = rhi::Sampler(pc.tex_sampler).get_nuri();

    float4 x_y_z_xdx = float4(0.,0.,0.,0.);
    float4 ydx_zdx_ydy_zdy = float4(0.,0.,0.,0.);
    for (uint i = 0; i < 4; ++i)
    {
        x_y_z_xdx += rhi::Texture(pc.x_y_z_xdx_tex).sample_level_2d_array_uniform<float4>(tex_sampler, ps_in.uvs[i], 0, 1);
        ydx_zdx_ydy_zdy += rhi::Texture(pc.ydx_zdx_ydy_zdy_tex).sample_level_2d_array_uniform<float4>(tex_sampler, ps_in.uvs[i], 0, 1);
    }

    float x_dx = x_y_z_xdx[3];
    float y_dx = ydx_zdx_ydy_zdy[0];
    float z_dx = ydx_zdx_ydy_zdy[1];
    float y_dy = ydx_zdx_ydy_zdy[2];
    float z_dy = ydx_zdx_ydy_zdy[3];

    float3 normal = calculate_normals(calculate_slope(z_dx, z_dy, x_dx, y_dy));
    float3 light_dir = float3(0.,-1.,0.);
    float ndotl = saturate(dot(normal, light_dir));
    
    float3 color = float3(0, 0.08866, 0.29177);
    float3 ambient = 0.55 * color;
    float3 diffuse = ndotl * color;

    PS_Out result = { float4(saturate(ambient + diffuse),1.) };
    return result;
}
