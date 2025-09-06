#ifndef REN_PBR
#define REN_PBR

#include "constants.hlsli"

namespace ren
{
namespace pbr
{
float D_GGX_ThrowbridgeReitz(float NdotH, float alpha)
{
    float NdotH2 = NdotH * NdotH;
    float alpha2 = alpha * alpha;
    float nom = alpha2;
    float denom = ren::PI * pow(NdotH2 * (alpha2 - 1.0) + 1.0, 2.0);
    return nom / denom;
}

float G_GGX1(float AdotB, float k)
{
    float nom = AdotB;
    float denom = AdotB * (1.0 - k) + k;
    return nom / denom;
}

float G_GGX1_k(float roughness)
{
    float k = roughness + 1.0;
    return k * k / 8.0;
}

float G_GGX1_k_ibl(float roughness)
{
    return (roughness * roughness) / 2.0;
}

float G_GGX(float NdotL, float NdotV, float roughness)
{
    float k = G_GGX1_k(roughness);
    return G_GGX1(NdotL, k) * G_GGX1(NdotV, k);
}

float G_GGX_ibl(float NdotL, float NdotV, float roughness)
{
    float k = G_GGX1_k_ibl(roughness);
    return G_GGX1(NdotL, k) * G_GGX1(NdotV, k);
}

float3 F_SphericalGaussian(float VdotH, float3 F0)
{
    return F0 + (1.0 - F0) * pow(2.0, -5.55473 * VdotH - 6.98316 * VdotH);
}

float3 BRDF_Diffuse_Lambert(float3 albedo)
{
    // No scaling by 1/pi.
    // https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return albedo;
}

float3 BRDF_Specular_CookTorrance(float NdotL, float NdotV, float NdotH, float VdotH, float roughness, float3 F0)
{
    float D = D_GGX_ThrowbridgeReitz(NdotH, roughness);
    float3 F = F_SphericalGaussian(VdotH, F0);
    float G = G_GGX(NdotL, NdotV, roughness);
    float3 nom = D * F * G;
    float denom = 4.0 * NdotL * NdotV;
    return nom / max(denom, 0.00001);
}

float3 BRDF_Specular_CookTorrance_ibl(float NdotL, float NdotV, float NdotH, float VdotH, float roughness, float3 F0)
{
    float D = D_GGX_ThrowbridgeReitz(NdotH, roughness);
    float3 F = F_SphericalGaussian(VdotH, F0);
    float G = G_GGX_ibl(NdotL, NdotV, roughness);
    float3 nom = D * F * G;
    float denom = 4.0 * NdotL * NdotV;
    return nom / max(denom, 0.00001);
}

} // namespace pbr
} // namespace ren

#endif
