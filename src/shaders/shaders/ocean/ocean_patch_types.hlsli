#ifndef REN_OCEAN_PATCH_RENDER_TYPES
#define REN_OCEAN_PATCH_RENDER_TYPES

struct VS_Out
{
    float4 position : SV_Position;
    float2 uvs[4] : TEXCOORD;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 color : SV_Target;
};

#endif
