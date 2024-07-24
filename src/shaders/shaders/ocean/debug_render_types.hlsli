#ifndef REN_OCEAN_DEBUG_RENDER_TYPES
#define REN_OCEAN_DEBUG_RENDER_TYPES

struct VS_Out
{
    float4 position : SV_Position;
    float4 color : SV_COLOR0;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 color : SV_Target;
};

#endif
