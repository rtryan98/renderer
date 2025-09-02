#ifndef REN_SAMPLING
#define REN_SAMPLING

#include "constants.hlsli"

namespace ren
{
namespace sampling
{
namespace sequences
{
float2 Hammersley(uint i, uint sample_count)
{
    uint a = i;
    a = (a << 16) | (a >> 16);
    a = ((a & 0x55555555u) << 1) | ((a & 0xAAAAAAAAu) >> 1);
    a = ((a & 0x33333333u) << 2) | ((a & 0xCCCCCCCCu) >> 2);
    a = ((a & 0x0F0F0F0Fu) << 4) | ((a & 0xF0F0F0F0u) >> 4);
    a = ((a & 0x00FF00FFu) << 8) | ((a & 0xFF00FF00u) >> 8);
    float radical_inverse = float(a) * 2.3283064365386963e-10; // 0x100000000
    return float2(float(i) / float(sample_count), radical_inverse);
}
} // namespace sequences

float3 hemisphere_uniform(uint i, uint sample_count)
{
    float2 xi = sequences::Hammersley(i, sample_count);
    float phi = xi.x * ren::TWO_PI;
    float cos_theta = 1.0 - xi.y;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    float3 result = float3(0.0, 0.0, cos_theta);
    sincos(phi, result.y, result.x);
    result.xy *= sin_theta;
    return result;
}

float3 hemisphere_cosine_weighted(uint i, uint sample_count)
{
    float2 xi = sequences::Hammersley(i, sample_count);
    float phi = xi.x * ren::TWO_PI;
    float cos_theta = sqrt(1.0 - xi.y);
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    float3 result = float3(0.0, 0.0, cos_theta);
    sincos(phi, result.y, result.x);
    result.xy *= sin_theta;
    return result;
}

float3 hemisphere_ggx(uint i, uint sample_count, float roughness)
{
    float alpha = roughness * roughness;
    float2 xi = sequences::Hammersley(i, sample_count);
    float phi = xi.x * ren::TWO_PI;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    float3 result = float3(0.0, 0.0, cos_theta);
    sincos(phi, result.y, result.x);
    result.xy *= sin_theta;
    return result;
}

} // namespace sampling
} // namespace ren

#endif
