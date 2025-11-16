#ifndef TONEMAP_SHARED_TYPES
#define TONEMAP_SHARED_TYPES
#include "shared/shared_types.h"

struct Tone_Map_Push_Constants
{
    SHADER_HANDLE_TYPE source_texture;
    SHADER_HANDLE_TYPE texture_sampler;
    SHADER_HANDLE_TYPE tone_map_parameters_buffer;
    uint is_enabled;
};

struct GT7_Tone_Mapping_Data
{
    // EOTF and display
    uint is_hdr;
    float reference_luminance;

    // Curve data
    float alpha;
    float mid_point;
    float linear_section;
    float toe_strength;
    float kA;
    float kB;
    float kC;

    // Tone mapping data
    float sdr_correction_factor;
    float luminance_target;
    float luminance_target_ICtCp;
    float luminance_target_Jzazbz;
    float blend_ratio;
    float fade_start;
    float fade_end;
};

struct Tone_Map_Debug_Quads_Push_Constants
{
    float aspect;
};

#endif
