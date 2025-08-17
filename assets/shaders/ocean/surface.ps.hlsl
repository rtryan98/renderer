#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shaders/ocean/ocean_patch_types.hlsli"
#include "rhi/bindless.hlsli"
#include "constants.hlsli"
#include "ocean/ocean_render_utils.hlsli"
#include "common/pbr/pbr.hlsli"
#include "common/color/color_spaces.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Render_Patch_Push_Constants, pc);

struct Surface
{
    float3 albedo;
    float3 normal;
    float metallic;
    float roughness;
};

struct Directional_Light
{
    float3 color;
    float3 direction;
};

// All vectors and directions are in WORLD SPACE.
float3 evaluate_light(Directional_Light light, float3 V, Surface surface)
{
    float3 L = light.direction;
    float3 N = surface.normal;
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), surface.albedo, surface.metallic);
    float3 kd = lerp(1.0 - ren::pbr::F_SphericalGaussian(VdotH, F0), 0.0, surface.metallic);
    float3 diffuse = kd * ren::pbr::BRDF_Diffuse_Lambert(surface.albedo);
    float3 specular = ren::pbr::BRDF_Specular_CookTorrance(NdotL, NdotV, NdotH, VdotH, surface.roughness, F0);

    if (NdotL > 0.0) // ignore all divisions by 0
        return (diffuse + specular) * NdotL * light.color;
    else
        return 0.0;
}

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

    float3 diffuse_color = float3(0.02352941176, 0.25882352941, 0.45098039215);
    Surface surface;
    surface.albedo = ren::color::spaces::Rec709_Rec2020(diffuse_color);
    surface.normal = calculate_normals(calculate_slope(z_dx, z_dy, x_dx, y_dy));
    surface.metallic = 0.0;
    surface.roughness = 0.25;

    float3 V = normalize(ps_in.position_camera - ps_in.position_ws).xyz;
    if (dot(V, surface.normal) < 0.0) surface.normal *= -1.;

    Directional_Light light;
    light.color = 1.0;
    light.direction = normalize(float3(0.8, 0.8, 1.0));
    return float4(evaluate_light(light, V, surface), 1.0);
}
