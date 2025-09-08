#ifndef MIPMAP_GEN_SHARED_TYPES
#define MIPMAP_GEN_SHARED_TYPES
#include "shared/shared_types.h"

struct Mipmap_Gen_Push_Constants
{
    SHADER_HANDLE_TYPE src;
    SHADER_HANDLE_TYPE dst;
    uint is_array;
};

#endif
