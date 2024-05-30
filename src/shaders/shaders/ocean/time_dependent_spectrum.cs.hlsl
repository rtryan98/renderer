#include "shaders/ocean/oceanography.hlsli"
#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Time_Dependent_Spectrum_Push_Constants, pc);

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    rhi::RW_Texture spectrum_tex = { pc.spectrum_tex };
    spectrum_tex.store_2d_array_uniform(id, float4(1., 0., 1., 0.));
}
