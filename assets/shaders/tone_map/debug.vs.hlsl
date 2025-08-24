#include "rhi/bindless.hlsli"
#include "shared/camera_shared_types.h"
#include "shared/tone_map_shared_types.h"

DECLARE_PUSH_CONSTANTS(Tone_Map_Debug_Quads_Push_Constants, pc);

struct VS_Out
{
    float4 position : SV_Position;
    float hue : COLOR0;
    float value : COLOR1;
};

float4x4 ortho(float left, float top, float right, float bottom)
{
    return transpose(float4x4(
        float4(2.0 / (right - left)           , 0.0                            , 0.0, 0.0),
        float4(0.0                            , 2.0 / (top - bottom)           , 0.0, 0.0),
        float4(0.0                            , 0.0                            , 0.5, 0.0),
        float4((right + left) / (left - right), (top + bottom) / (bottom - top), 0.5, 1.0)));
}

VS_Out main(uint vertex_id : SV_VertexID)
{
    uint idx = vertex_id % 6;
    static const float X_SIZE = 1.0;
    static const float Y_SIZE = 0.5;
    static const float2 POSITIONS[6] = {
        float2(-X_SIZE, -Y_SIZE),
        float2( X_SIZE, -Y_SIZE),
        float2( X_SIZE,  Y_SIZE),
        float2( X_SIZE,  Y_SIZE),
        float2(-X_SIZE,  Y_SIZE),
        float2(-X_SIZE, -Y_SIZE)
    };

    float4 position = float4(POSITIONS[idx], 0.0, 1.0);
    position.y *= pc.aspect;

    VS_Out result = { position, POSITIONS[idx].y < 0.0 ? 0.0 : 1.0, POSITIONS[idx].x < 0.0 ? -3.0 : 6.0 };
    return result;
}
