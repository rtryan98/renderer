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

float3 evaluate_punctual_lights(float3 V, Surface surface, float3 F0)
{
    Scene_Info scene_info = rhi::uni::buf_load<Scene_Info>(REN_GLOBAL_SCENE_INFORMATION_BUFFER);
    float3 color = 0.0;

    for (uint i = 0; i < scene_info.light_count; ++i)
    {
        Punctual_Light light = rhi::uni::buf_load_arr<Punctual_Light>(REN_GLOBAL_LIGHT_LIST_BUFFER, i);
        if (light.disabled == 1)
            continue;

        float3 light_color = ren::unpack_unorm_4x8(light.color).xyz;

        switch(light.type)
        {
        case Light_Type::Point:
        {
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

            if (NdotL > 0.0)
            {
                color += (diffuse + specular) * NdotL * light_color * light.intensity * attenuation;
            }
            break;
        }
        case Light_Type::Spot:
        {
            // Not yet implemented
            break;
        }
        case Light_Type::Directional:
        {
            float3 L = -light.direction;
            float3 H = normalize(L + V);

            float NdotL = saturate(dot(surface.normal, L));
            float NdotV = saturate(dot(surface.normal, V));
            float NdotH = saturate(dot(surface.normal, H));
            float VdotH = saturate(dot(V, H));

            float3 kd = lerp(1.0 - F_SphericalGaussian(VdotH, F0), 0.0, surface.metallic);
            float3 diffuse = kd * BRDF_Diffuse_Lambert(surface.albedo);
            float3 specular = BRDF_Specular_CookTorrance(NdotL, NdotV, NdotH, VdotH, surface.roughness, F0);

            if (NdotL > 0.0)
            {
                color += (diffuse + specular) * NdotL * light_color * light.intensity;
            }
        }
        }
    }

    return color;
}

float3 evaluate_skylight(float3 V, Surface surface, float3 F0)
{
    float3 color = 0.0;

    color += 0.075 * surface.albedo; // constant ambient factor.

    return color;
}

float3 evaluate_lights(float3 V, Surface surface)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), surface.albedo, surface.metallic);
    float3 color = 0.0;

    color += evaluate_punctual_lights(V, surface, F0);

    color += evaluate_skylight(V, surface, F0);

    return color;
}
}
}

#endif
