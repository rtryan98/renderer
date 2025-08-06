#ifndef G_BUFFER_SHARED_TYPES
#define G_BUFFER_SHARED_TYPES
#include "shared/shared_types.h"

struct G_Buffer_Resolve_Push_Constants
{
    SHADER_HANDLE_TYPE albedo;
    SHADER_HANDLE_TYPE normals;
    SHADER_HANDLE_TYPE metallic_roughness;
    SHADER_HANDLE_TYPE depth;
    SHADER_HANDLE_TYPE resolve_target;
    SHADER_HANDLE_TYPE texture_sampler;
    SHADER_HANDLE_TYPE camera_buffer;
    uint width;
    uint height;
};

#endif
