#ifndef REN_PBR
#define REN_PBR

#include "constants.hlsli"

namespace ren
{
namespace pbr
{
float D_GGX_ThrowbridgeReitz(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float f = (NdotH * a2 - NdotH) * NdotH + 1.;
    return a2 / (f * f);
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
    return roughness * roughness;
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

float G_SmithGGX_Correlated(float NdotL, float NdotV, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float lambda_GGX_V = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
    float lambda_GGX_L = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);

    return .5 / (lambda_GGX_V + lambda_GGX_L);
}

float3 F_Schlick(float3 f0, float f90, float u)
{
    return f0 + (f90 - f0) * pow(1. - u, 5.);
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

float BRDF_Diffuse_Disney(float NdotL, float NdotV, float LdotH, float roughness)
{
    float energy_bias = lerp(.0, .5, roughness);
    float energy_factor = lerp(1., 1. / 1.51, roughness);
    float fd90 = energy_bias + 2. * LdotH * LdotH * roughness;
    float3 f0 = 1.;
    float light_scatter = F_Schlick(f0, fd90, NdotL).r;
    float view_scatter = F_Schlick(f0, fd90, NdotV).r;
    return light_scatter * view_scatter * energy_factor / ren::PI;
}

float3 BRDF_Specular_CookTorrance(float D, float3 F, float G, float NdotL, float NdotV)
{
    return D * F * G / ren::PI;
}

float3 BRDF_Specular_CookTorrance(float NdotL, float NdotV, float NdotH, float VdotH, float roughness, float3 F0)
{
    float D = D_GGX_ThrowbridgeReitz(NdotH, roughness);
    float3 F = F_SphericalGaussian(VdotH, F0);
    float G = G_SmithGGX_Correlated(NdotL, NdotV, roughness);

    return BRDF_Specular_CookTorrance(D, F, G, NdotL, NdotV);
}

float3 BRDF_Specular_CookTorrance_ibl(float NdotL, float NdotV, float NdotH, float VdotH, float roughness, float3 F0)
{
    float D = D_GGX_ThrowbridgeReitz(NdotH, roughness);
    float3 F = F_SphericalGaussian(VdotH, F0);
    float G = G_SmithGGX_Correlated(NdotL, NdotV, roughness);

    return BRDF_Specular_CookTorrance(D, F, G, NdotL, NdotV);
}

float3 BRDF_Specular_Mirror(float LdotR, float3 F0)
{
    // TODO: multiply with F0? Not sure how to handle diffuse here
    return F0 * step(.99, LdotR);
}

} // namespace pbr
} // namespace ren

#endif
