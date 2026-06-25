#ifndef REN_PROJECTION_HLSLI
#define REN_PROJECTION_HLSLI

namespace ren
{
float2 uv_from_thread_id(uint2 id, uint2 image_size)
{
    return float2(0.5 + float2(id.xy)) / float2(image_size);
}

float3 position_from_depth(float4x4 clip_to_world, float2 uv, float depth)
{
    float4 position = float4(uv * 2.0 - 1.0, depth, 1.0); // NDC
    position.y *= -1;
    position = mul(clip_to_world, position);
    return position.xyz / position.w;
}
}

#endif
