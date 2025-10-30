#ifndef REN_SCENE_SHARED_TYPES
#define REN_SCENE_SHARED_TYPES

#include "shared/shared_types.h"

enum SHADER_ENUM_CLASS Light_Type
{
    Point = 0,
    Spot = 1,
    Directional = 2
};

struct Punctual_Light
{
    uint disabled : 1;
    uint type : 7;
    uint color : 24;

    float intensity;

    // Only for point lights and spotlights.
    float3 position;

    // Only for spotlights and directional lights
    float3 direction;

    // For point lights: range
    // For spotlights: inner cone angle, outer cone angle
    float2 arguments;
};

struct Scene_Info
{
    uint light_count;
    SHADER_HANDLE_TYPE tlas;

    float3 sun_direction;
    float sun_intensity;
};

#endif
