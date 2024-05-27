#ifndef FFT_SHARED_TYPES
#define FFT_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN FFT_Push_Constants
{
    SHADER_HANDLE_TYPE image;
    uint inverted;
};

#endif
