#include "shaders/ocean/oceanography.hlsli"
#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/complex.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Time_Dependent_Spectrum_Push_Constants, pc);

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 spectrum_k = rhi::uni::tex_load_arr<float4>(pc.initial_spectrum_tex, id.xy, id.z);
    float2 spectrum = spectrum_k.xy;
    float2 k = spectrum_k.zw;
    float rcp_wavenumber = rcp(max(0.001, length(k)));
    float2 spectrum_minus_k = ren::cconjugate(rhi::uni::tex_load_arr<float4>(pc.initial_spectrum_tex, uint2((pc.texture_size - id.x) % pc.texture_size, (pc.texture_size - id.y) % pc.texture_size), id.z).xy);
    float omega_k = rhi::uni::tex_load_arr<float>(pc.angular_frequency_tex, id.xy, id.z);

    float2 arg = ren::cpolar(1., pc.time * omega_k);
    float2 h = ren::cadd(ren::cmul(spectrum, arg), ren::cmul(spectrum_minus_k, ren::cconjugate(arg)));
    float2 ih = ren::cmuli(h);

    float2 x    =  ih * k.x * rcp_wavenumber;
    float2 y    =  ih * k.y * rcp_wavenumber;
    float2 z    =   h;
    float2 xdx  = - h * k.x * k.x * rcp_wavenumber;
    float2 ydx  = - h * k.y * k.x * rcp_wavenumber;
    float2 zdx  =  ih * k.x;
    float2 ydy  = - h * k.y * k.y * rcp_wavenumber;
    float2 zdy  =  ih * k.y;

    uint2 shifted_pos = uint2((id.xy + uint2(pc.texture_size,pc.texture_size) / 2) % pc.texture_size);

    rhi::uni::tex_store_arr(pc.x_y_z_xdx_tex,       shifted_pos, id.z, float4(ren::cadd(  x, ren::cmuli(y)  ), ren::cadd(  z, ren::cmuli(xdx))));
    rhi::uni::tex_store_arr(pc.ydx_zdx_ydy_zdy_tex, shifted_pos, id.z, float4(ren::cadd(ydx, ren::cmuli(zdx)), ren::cadd(ydy, ren::cmuli(zdy))));
}
