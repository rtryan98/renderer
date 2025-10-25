#ifndef REN_IMGUI
#define REN_IMGUI

#include "rhi/bindless.hlsli"

struct Imgui_Push_Constants
{
    uint vertex_buffer_idx;
    uint texture_idx;
    float left;
    float top;
    float right;
    float bottom;
};

struct Imgui_Vert
{
    float2 pos;
    float2 uv;
    uint col;
};

struct VS_Out
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};

typedef VS_Out PS_In;

struct PS_Out
{
    float4 col : SV_TARGET0;
};

float4x4 ortho(float left, float top, float right, float bottom)
{
    return float4x4(
        float4(2.0 / (right - left)           , 0.0                            , 0.0, 0.0),
        float4(0.0                            , 2.0 / (top - bottom)           , 0.0, 0.0),
        float4(0.0                            , 0.0                            , 0.5, 0.0),
        float4((right + left) / (left - right), (top + bottom) / (bottom - top), 0.5, 1.0));
}

#endif
