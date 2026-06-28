#ifndef G_BUFFER_SHARED_TYPES
#define G_BUFFER_SHARED_TYPES
#include "shared/shared_types.h"

struct G_Buffer_Resolve_Push_Constants
{
    SHADER_HANDLE_TYPE g_buffer_0; // R8G8B8A8 SRGB [albedo.xyz, 1.0]
    SHADER_HANDLE_TYPE g_buffer_1; // R10G10B10A2 [oct_n.x, oct_n.y, 0., oct_n.z]
    SHADER_HANDLE_TYPE g_buffer_2; // R8G8 [metallic, roughness]
    SHADER_HANDLE_TYPE g_buffer_3; // R16G16 [mv]
    SHADER_HANDLE_TYPE depth;
    SHADER_HANDLE_TYPE resolve_target;
    SHADER_HANDLE_TYPE camera_buffer;
    uint width;
    uint height;
};

#endif
