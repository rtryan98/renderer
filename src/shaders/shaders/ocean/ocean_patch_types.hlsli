#ifndef REN_OCEAN_PATCH_RENDER_TYPES
#define REN_OCEAN_PATCH_RENDER_TYPES

struct VS_Out
{
    float4 position : SV_Position;
    float2 uvs[4] : TEXCOORD;
    float4 position_ws : POSITION0;
    float4 position_camera : POSITION1;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 color : SV_Target;
};

#endif
