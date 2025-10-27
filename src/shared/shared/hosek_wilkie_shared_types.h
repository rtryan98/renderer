#ifndef HOSEK_WILKIE_SHARED_TYPES
#define HOSEK_WILKIE_SHARED_TYPES
#include "shared/shared_types.h"

struct Hosek_Wilkie_Parameters
{
    float4 values[9];
    float4 radiance;
};

struct Hosek_Wilkie_Cubemap_Gen_Push_Constants
{
    float4 sun_direction;
    SHADER_HANDLE_TYPE parameters_buffer;
    SHADER_HANDLE_TYPE target_cubemap;
    uint image_size;
    uint use_xyz;
};

#endif
