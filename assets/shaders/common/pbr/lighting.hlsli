#ifndef REN_LIGHTING
#define REN_LIGHTING

#include "rhi/bindless.hlsli"
#include "shared/shared_resources.h"
#include "shared/scene_shared_types.h"

#include "util.hlsli"
#include "common/pbr/pbr.hlsli"

namespace ren
{
namespace pbr
{
struct Surface
{
    float3 position;
    float3 albedo;
    float3 normal;
    float metallic;
    float roughness;
};

float distance_squared(float3 B, float3 A)
{
    float3 d = B - A;
    return dot(d, d);
}

float3 evaluate_point_light(float3 V, Surface surface, float3 F0, Punctual_Light light)
{
    float3 light_color = ren::unpack_unorm_4x8(light.color).xyz;

    float distance2 = distance_squared(surface.position, light.position);
    float3 L = normalize(light.position - surface.position);
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(surface.normal, L));
    float NdotV = saturate(dot(surface.normal, V));
    float NdotH = saturate(dot(surface.normal, H));
    float VdotH = saturate(dot(V, H));

    float3 kd = lerp(1.0 - F_SphericalGaussian(VdotH, F0), 0.0, surface.metallic);
    float3 diffuse = kd * BRDF_Diffuse_Lambert(surface.albedo);
    float3 specular = BRDF_Specular_CookTorrance(NdotL, NdotV, NdotH, VdotH, surface.roughness, F0);

    float attenuation = 1.0 / (distance2 * 2.0 * ren::TWO_PI);

    return NdotL > 0.0 ? (diffuse + specular) * NdotL * light_color * light.intensity * attenuation : 0.0;
}

float3 evaluate_spotlight(float3 V, Surface surface, float3 F0, Punctual_Light light)
{
    // TODO: not yet implemented.
    return 0.0;
}

float3 evaluate_directional_light(float3 V, Surface surface, float3 F0, Punctual_Light light)
{
    float3 light_color = ren::unpack_unorm_4x8(light.color).xyz;

    float3 L = -light.direction;
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(surface.normal, L));
    float NdotV = saturate(dot(surface.normal, V));
    float NdotH = saturate(dot(surface.normal, H));
    float VdotH = saturate(dot(V, H));

    float3 ks = lerp(F_SphericalGaussian(VdotH, F0), 0.0, surface.metallic);
    float3 kd = 1.0 - ks;
    float3 diffuse = kd * BRDF_Diffuse_Lambert(surface.albedo);
    float3 specular = ks * BRDF_Specular_CookTorrance(NdotL, NdotV, NdotH, VdotH, surface.roughness, F0);

    return NdotL > 0.0 ? (diffuse + specular) * NdotL * light_color * light.intensity : 0.0;
}

float3 evaluate_punctual_lights(float3 V, Surface surface, float3 F0)
{
    Scene_Info scene_info = rhi::uni::buf_load<Scene_Info>(REN_GLOBAL_SCENE_INFORMATION_BUFFER);
    float3 color = 0.0;

    for (uint i = 0; i < scene_info.light_count; ++i)
    {
        Punctual_Light light = rhi::uni::buf_load_arr<Punctual_Light>(REN_GLOBAL_LIGHT_LIST_BUFFER, i);
        if (light.disabled == 1)
            continue;

        switch(light.type)
        {
        case Light_Type::Point:
        {
            color += evaluate_point_light(V, surface, F0, light);
            break;
        }
        case Light_Type::Spot:
        {
            // Not yet implemented
            break;
        }
        case Light_Type::Directional:
        {
            color += evaluate_directional_light(V, surface, F0, light);
            break;
        }
        }
    }

    return color;
}

float3 evaluate_skylight_ibl(float3 V, Surface surface, float3 F0)
{
    float3 color = 0.0;

    float NdotV = saturate(dot(surface.normal, V));

    float3 ks = lerp(F_SphericalGaussian(NdotV, F0), 0.0, surface.metallic);
    float3 kd = 1.0 - ks;
    float3 diffuse_irradiance = rhi::uni::tex_sample_level_cube<float4>(REN_LIGHTING_DIFFUSE_IRRADIANCE_CUBEMAP, REN_SAMPLER_LINEAR_WRAP, surface.normal, 0.0).xyz;
    float3 diffuse = kd * diffuse_irradiance * surface.albedo;

    float3 specular_irradiance = rhi::uni::tex_sample_level_cube<float4>(REN_LIGHTING_SPECULAR_IRRADIANCE_CUBEMAP, REN_SAMPLER_LINEAR_WRAP, surface.normal, 4.0 * surface.roughness).xyz;
    float2 specular_brdf_lut = rhi::uni::tex_sample_level<float2>(REN_LIGHTING_BRDF_LUT_TEXTURE, REN_SAMPLER_LINEAR_CLAMP, float2(NdotV, surface.roughness), 0.0);
    float3 specular = (F0 * specular_brdf_lut.x + specular_brdf_lut.y) * specular_irradiance;

    color += diffuse;
    color += specular;

    return color;
}

float3 evaluate_lights(float3 V, Surface surface)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), surface.albedo, surface.metallic);
    float3 color = 0.0;

    color += evaluate_punctual_lights(V, surface, F0);

    color += evaluate_skylight_ibl(V, surface, F0);

    return color;
}
}
}

#endif
