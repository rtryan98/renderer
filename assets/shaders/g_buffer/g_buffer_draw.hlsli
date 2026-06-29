#ifndef G_BUFFER_DRAW_HLSLI
#define G_BUFFER_DRAW_HLSLI

struct VS_Out
{
    float4 position : SV_Position;
    float4 position_clip : POSITION0;
    float4 prev_position_clip : POSITION1;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
    nointerpolation uint material_index : INSTANCE;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 gbuffer0 : SV_Target0; // R8G8B8A8 SRGB [albedo.xyz, 1.0]
    float4 gbuffer1 : SV_Target1; // R10G10B10A2 [oct_n.x, oct_n.y, 0., oct_n.z]
    float2 gbuffer2 : SV_Target2; // R8G8 [metallic, roughness]
    float2 gbuffer3 : SV_Target3; // R16G16 [mv]
};

#endif
