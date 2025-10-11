#ifndef REN_OCEAN_PATCH_RENDER_TYPES
#define REN_OCEAN_PATCH_RENDER_TYPES

struct VS_Out
{
    float4 position : SV_Position;
    float2 uvs[4] : TEXCOORD0;
    float3 position_ws : POSITION1;
};

typedef VS_Out PS_In;

#endif
