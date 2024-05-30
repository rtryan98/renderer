#ifndef OCEAN_SHARED_TYPES
#define OCEAN_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN Ocean_Spectrum_Data
{

};

struct SHADER_STRUCT_ALIGN Ocean_Initial_Spectrum_Data
{
    Ocean_Spectrum_Data spectra[2];
};

struct SHADER_STRUCT_ALIGN Ocean_Initial_Spectrum_Push_Constants
{
    SHADER_HANDLE_TYPE data;
    SHADER_HANDLE_TYPE spectrum_tex;
};

struct SHADER_STRUCT_ALIGN Ocean_Time_Dependent_Spectrum_Data
{
    
};

struct SHADER_STRUCT_ALIGN Ocean_Time_Dependent_Spectrum_Push_Constants
{
    SHADER_HANDLE_TYPE spectrum_tex;
};

#endif
