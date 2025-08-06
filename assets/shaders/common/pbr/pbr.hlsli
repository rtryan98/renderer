#ifndef REN_PBR
#define REN_PBR

#include "constants.hlsli"

namespace ren
{
namespace pbr
{
float NDF_GGX(float3 N, float3 H, float a)
{
    float NdotH = saturate(dot(N, H));
    float denom = (NdotH * NdotH * ((a * a) - 1.0) + 1.0);
    return (a * a) / (ren::PI * denom * denom);
}

float G_SchlickGGX(float AdotB, float k)
{
    return AdotB / (AdotB * (1.0 - k) + k);
}

float G_Smith(float3 N, float3 V, float3 L, float k)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

float3 Fresnel_Schlick(float cos_theta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float3 BRDF_CookTorrance(float3 L, float3 N, float3 V, float metallic, float roughness, float3 F0, float3 albedo)
{
    float3 H = normalize(L + V);
    float NDF = NDF_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    float3 F = Fresnel_Schlick(saturate(dot(H, V)), F0);
    return ((1.0 - F) * (1.0 - metallic)) * albedo / ren::PI + (NDF * G * F / max(4.0 * saturate(dot(N, V)) * saturate(dot(N, L)), 0.0001));
}

} // namespace pbr
} // namespace ren

#endif
