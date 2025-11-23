#ifndef EXPOSURE_SHARED_TYPES
#define EXPOSURE_SHARED_TYPES
#include "shared/shared_types.h"

struct Luminance_Histogram
{
    float average_luminance;
    float buckets[256];
};

struct Calculate_Luminance_Histogram_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE source_image;
    SHADER_HANDLE_TYPE luminance_histogram_buffer;
};

struct Calculate_Average_Luminance_Push_Constants
{
    SHADER_HANDLE_TYPE luminance_histogram_buffer;
    float delta_time;
    float tau;
};

struct Apply_Exposure_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE image;
    SHADER_HANDLE_TYPE luminance_histogram_buffer;
    uint use_camera_exposure;
    float aperture;
    float shutter;
    float iso;
};

#endif
