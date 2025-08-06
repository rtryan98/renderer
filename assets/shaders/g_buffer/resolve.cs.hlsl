#include "shared/g_buffer_shared_types.h"
#include "shared/camera_shared_types.h"
#include "rhi/bindless.hlsli"
#include "common/pbr/pbr.hlsli"

DECLARE_PUSH_CONSTANTS(G_Buffer_Resolve_Push_Constants, pc);

struct Directional_Light
{
    float3 color;
    float3 direction;
};

// All vectors and directions are in WORLD SPACE.
float3 resolve_radiance(Directional_Light light, float3 camera_position, float3 position, float3 albedo, float3 normal, float metallic, float roughness)
{
    float3 L = light.direction;
    float3 N = normal;
    float3 V = normalize(camera_position - position);
    float3 radiance = ren::pbr::BRDF_CookTorrance(L, N, V, metallic, roughness, lerp(float3(0.04, 0.04, 0.04), albedo, metallic), albedo);
    return radiance * saturate(dot(N, L)) * light.color;
}

float3 position_from_depth(GPU_Camera_Data camera, float2 uv, float depth)
{
    float4 position = float4(uv * 2.0 - 1.0, depth, 1.0); // NDC
    position.y *= -1;
    position = mul(camera.clip_to_world, position);
    return position.xyz / position.w;
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= pc.width || id.y >= pc.height)
        return;
    float2 uv = (0.5 + float2(id.xy)) / float2(pc.width, pc.height);
    float depth = rhi::uni::tex_sample_level<float>(pc.depth, pc.texture_sampler, uv, 0.).x;
    GPU_Camera_Data camera = rhi::uni::buf_load<GPU_Camera_Data>(pc.camera_buffer);
    float3 position = position_from_depth(camera, uv, depth);

    float3 albedo = rhi::uni::tex_sample_level<float4>(pc.albedo, pc.texture_sampler, uv, 0.).xyz;
    float3 normal = rhi::uni::tex_sample_level<float4>(pc.normals, pc.texture_sampler, uv, 0.).xyz;
    float2 metallic_roughness = rhi::uni::tex_sample_level<float2>(pc.metallic_roughness, pc.texture_sampler, uv, 0.);

    Directional_Light light;
    light.color = 1.0;
    light.direction = normalize(float3(0.0, 0.0, 1.0));
    float4 result = float4(resolve_radiance(light, camera.position.xyz, position, albedo, normal, metallic_roughness.x, metallic_roughness.y), 1.0);
    // float4 result = 0.5 + 0.5 * float4(normal, 1.0);
    rhi::uni::tex_store(pc.resolve_target, id.xy, result);
}
