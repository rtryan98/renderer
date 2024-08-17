#ifndef CAMERA_SHARED_TYPES
#define CAMERA_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN GPU_Camera_Data
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
    float4   position;
};

#endif
