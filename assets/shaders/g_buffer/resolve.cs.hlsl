#include "shared/g_buffer_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "common/pbr/pbr.hlsli"

DECLARE_PUSH_CONSTANTS(G_Buffer_Resolve_Push_Constants, pc);

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

float3 position_from_depth(GPU_Camera_Data camera, float2 uv, float depth)
{
    float4 position = float4(uv * 2.0 - 1.0, depth, 1.0); // NDC
    position.y *= -1;
    position = mul(camera.clip_to_world, position);
    return position.xyz / position.w;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.width || id.y >= pc.height)
        return;
    float2 uv = (0.5 + float2(id.xy)) / float2(pc.width, pc.height);

    float depth = rhi::uni::tex_sample_level<float>(pc.depth, pc.texture_sampler, uv, 0.).x;
    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    float3 position = position_from_depth(camera, uv, depth);
    float3 V = normalize(camera.position.xyz - position);

    Surface surface;
    surface.albedo = rhi::uni::tex_sample_level<float4>(pc.albedo, pc.texture_sampler, uv, 0.).xyz;
    surface.normal = rhi::uni::tex_sample_level<float4>(pc.normals, pc.texture_sampler, uv, 0.).xyz;
    float2 metallic_roughness = rhi::uni::tex_sample_level<float2>(pc.metallic_roughness, pc.texture_sampler, uv, 0.);
    surface.metallic = metallic_roughness.x;
    surface.roughness = metallic_roughness.y;

    Directional_Light light;
    light.color = 1.0;
    light.direction = normalize(float3(0.8, 0.0, 1.0));

    float3 direct_light = evaluate_light(light, V, surface);

    float4 result = float4(direct_light, 1.0);
    rhi::uni::tex_store(pc.resolve_target, id.xy, result);
}
