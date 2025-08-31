#ifndef IBL_SHARED_TYPES
#define IBL_SHARED_TYPES
#include "shared/shared_types.h"

struct Equirectangular_To_Cubemap_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE source_image;
    SHADER_HANDLE_TYPE target_cubemap;
    SHADER_HANDLE_TYPE source_image_sampler;
};

struct Skybox_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE depth_buffer;
    SHADER_HANDLE_TYPE target_image;
    SHADER_HANDLE_TYPE cubemap;
    SHADER_HANDLE_TYPE cubemap_sampler;
    SHADER_HANDLE_TYPE camera_buffer;
};

#endif
