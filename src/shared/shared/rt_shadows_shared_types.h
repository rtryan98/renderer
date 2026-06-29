#ifndef RT_SHADOWS_SHARED_TYPES
#define RT_SHADOWS_SHARED_TYPES
#include "shared/shared_types.h"

struct RT_Shadows_Trace_Push_Constants
{
    uint2 image_size;
    SHADER_HANDLE_TYPE camera_buffer;
    SHADER_HANDLE_TYPE g_buffer_1_texture;
    SHADER_HANDLE_TYPE depth_texture;
    SHADER_HANDLE_TYPE visibility_output_texture;
};

#endif
