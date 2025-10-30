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

struct Prefilter_Diffuse_Irradiance_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE source_cubemap;
    SHADER_HANDLE_TYPE target_cubemap;
    uint samples;
};

struct Prefilter_Specular_Irradiance_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE source_cubemap;
    SHADER_HANDLE_TYPE target_cubemap;
    float roughness;
    uint samples;
};

struct BRDF_LUT_Bake_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE lut;
};

struct Skybox_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE depth_buffer;
    SHADER_HANDLE_TYPE target_image;
    SHADER_HANDLE_TYPE cubemap;
    SHADER_HANDLE_TYPE camera_buffer;
};

#endif
