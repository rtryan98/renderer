#ifndef BASIC_DRAW_HLSLI
#define BASIC_DRAW_HLSLI

struct VS_Out
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
    nointerpolation uint material_index : INSTANCE;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float2 metallic_roughness : SV_Target2;
};

#endif
