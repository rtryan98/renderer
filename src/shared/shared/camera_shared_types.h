#ifndef CAMERA_SHARED_TYPES
#define CAMERA_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN GPU_Camera_Data
{
    float4x4 world_to_camera;
    float4x4 camera_to_clip;
    float4x4 world_to_clip;
    float4x4 clip_to_camera;
    float4x4 camera_to_world;
    float4x4 clip_to_world;
    float4   position;
    float    near_plane;
    float    far_plane;
};

#endif
