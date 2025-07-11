#ifndef DRAW_SHARED_TYPES
#define DRAW_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN Immediate_Draw_Push_Constants
{
    SHADER_HANDLE_TYPE position_buffer;
    SHADER_HANDLE_TYPE attribute_buffer;
    SHADER_HANDLE_TYPE camera_buffer;
    uint vertex_offset;
};

#endif
