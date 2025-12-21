#ifndef EXPOSURE_SHARED_TYPES
#define EXPOSURE_SHARED_TYPES
#include "shared/shared_types.h"

struct Luminance_Histogram
{
    float average_luminance;
    uint buckets[256];
};

struct Calculate_Luminance_Histogram_Push_Constants
{
    uint image_width;
    uint image_height;
    SHADER_HANDLE_TYPE source_image;
    SHADER_HANDLE_TYPE luminance_histogram_buffer;
    float min_log_luminance;
    float log_luminance_range;
};

struct Calculate_Average_Luminance_Push_Constants
{
    SHADER_HANDLE_TYPE luminance_histogram_buffer;
    uint pixel_count;
    float delta_time;
    float tau;
    float min_log_luminance;
    float log_luminance_range;
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
    float auto_exposure_compensation;
};

#endif
