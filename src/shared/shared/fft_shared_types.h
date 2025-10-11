#ifndef FFT_SHARED_TYPES
#define FFT_SHARED_TYPES
#include "shared/shared_types.h"

#ifndef FFT_VERTICAL
#define FFT_VERTICAL 0
#endif
#ifndef FFT_HORIZONTAL
#define FFT_HORIZONTAL 1
#endif

struct SHADER_STRUCT_ALIGN FFT_Push_Constants
{
    SHADER_HANDLE_TYPE image;
    uint vertical_or_horizontal;
    uint inverse;
    SHADER_HANDLE_TYPE min_max_tex;
    uint min_max_tex_store_offset;
};

struct SHADER_STRUCT_ALIGN FFT_Min_Max_Resolve_Push_Constants
{
    SHADER_HANDLE_TYPE min_max_tex;
    uint min_max_tex_load_offset;
    SHADER_HANDLE_TYPE min_max_buffer;
    uint min_max_buffer_store_offset;
};

#endif
