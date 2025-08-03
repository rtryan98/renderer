#ifndef TONEMAP_SHARED_TYPES
#define TONEMAP_SHARED_TYPES
#include "shared/shared_types.h"

struct Tonemap_Push_Constants
{
    SHADER_HANDLE_TYPE source_texture;
    SHADER_HANDLE_TYPE texture_sampler;
};

#endif
