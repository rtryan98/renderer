#ifndef OCEAN_SHARED_TYPES
#define OCEAN_SHARED_TYPES
#include "shared/shared_types.h"

enum SHADER_ENUM_CLASS Ocean_Spectrum
{
    Phillips,
    Pierson_Moskowitz,
    Generalized_A_B,
    Jonswap,
    TMA
};

enum SHADER_ENUM_CLASS Ocean_Directional_Spreading_Function
{
    Positive_Cosine_Squared,
    Mitsuyasu,
    Hasselmann,
    Donelan_Banner,
    Flat
};

struct Ocean_Spectrum_Data
{
    float u;
    float f;
    float phillips_alpha;
    float generalized_a;
    float generalized_b;
    float contribution;
    float wind_direction;
};

struct SHADER_STRUCT_ALIGN Ocean_Initial_Spectrum_Data
{
    Ocean_Spectrum_Data spectra[2];
    uint4 active_cascades;
    float4 length_scales;
    uint spectrum;
    uint directional_spreading_function;
    uint texture_size;
    float g;
    float h;
};

struct SHADER_STRUCT_ALIGN Ocean_Initial_Spectrum_Push_Constants
{
    SHADER_HANDLE_TYPE data;
    SHADER_HANDLE_TYPE spectrum_tex;
    SHADER_HANDLE_TYPE angular_frequency_tex;
};

struct SHADER_STRUCT_ALIGN Ocean_Time_Dependent_Spectrum_Push_Constants
{
    SHADER_HANDLE_TYPE initial_spectrum_tex;
    SHADER_HANDLE_TYPE angular_frequency_tex;
    SHADER_HANDLE_TYPE x_y_z_xdx_tex;
    SHADER_HANDLE_TYPE ydx_zdx_ydy_zdy_tex;
    uint texture_size;
    float time;
};

struct SHADER_STRUCT_ALIGN Ocean_Render_Patch_Push_Constants
{
    float4 length_scales;
    SHADER_HANDLE_TYPE camera;
    SHADER_HANDLE_TYPE min_max_buffer;
    SHADER_HANDLE_TYPE packed_displacement_tex;
    SHADER_HANDLE_TYPE packed_derivatives_tex;
    SHADER_HANDLE_TYPE packed_xdx_tex;
    float cell_size;
    uint vertices_per_axis;
    float offset_x;
    float offset_y;
    uint lod_differences;
};

struct SHADER_STRUCT_ALIGN Ocean_Render_Composition_Push_Constants
{
    SHADER_HANDLE_TYPE ocean_color_tex;
    SHADER_HANDLE_TYPE ocean_depth_tex;
    SHADER_HANDLE_TYPE geom_color_tex;
    SHADER_HANDLE_TYPE geom_depth_tex;
    SHADER_HANDLE_TYPE tex_sampler;
};

struct SHADER_STRUCT_ALIGN Ocean_Min_Max_Values
{
    struct Cascade
    {
        float4 min_values;
        float4 max_values;
    };
    Cascade cascades[4];
};

struct SHADER_STRUCT_ALIGN Ocean_Reorder_Push_Constants
{
    SHADER_HANDLE_TYPE min_max_buffer;
    SHADER_HANDLE_TYPE x_y_z_xdx_tex;
    SHADER_HANDLE_TYPE ydx_zdx_ydy_zdy_tex;
    SHADER_HANDLE_TYPE displacement_tex;
    SHADER_HANDLE_TYPE derivatives_tex;
    SHADER_HANDLE_TYPE foam_tex;
};

#endif
