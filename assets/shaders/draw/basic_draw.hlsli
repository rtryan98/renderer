#ifndef BASIC_DRAW_HLSLI
#define BASIC_DRAW_HLSLI

struct VS_Out
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 color : SV_Target;
};

#endif
