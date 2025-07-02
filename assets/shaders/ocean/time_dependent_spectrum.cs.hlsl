#include "shaders/ocean/oceanography.hlsli"
#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/complex.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Time_Dependent_Spectrum_Push_Constants, pc);

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    rhi::Texture initial_spectrum_tex = { pc.initial_spectrum_tex };
    rhi::Texture angular_frequency_tex = { pc.angular_frequency_tex };
    rhi::RW_Texture x_y_z_xdx_tex = { pc.x_y_z_xdx_tex };
    rhi::RW_Texture ydx_zdx_ydy_zdy_tex = { pc.ydx_zdx_ydy_zdy_tex };

    float4 spectrum_k = initial_spectrum_tex.load_2d_array_uniform<float4>(id.xyz);
    float2 spectrum = spectrum_k.xy;
    float2 k = spectrum_k.zw;
    float rcp_wavenumber = rcp(max(0.001, length(k)));
    float2 spectrum_minus_k = ren::cconjugate(initial_spectrum_tex.load_2d_array_uniform<float4>(uint3((pc.texture_size - id.x) % pc.texture_size, (pc.texture_size - id.y) % pc.texture_size, id.z)).xy);
    float omega_k = angular_frequency_tex.load_2d_array_uniform<float>(id);

    float2 arg = ren::cpolar(1., pc.time * omega_k);
    float2 h = .5 * (ren::cadd(ren::cmul(spectrum, arg), ren::cmul(spectrum_minus_k, ren::cconjugate(arg))));
    float2 ih = ren::cmuli(h);

    float2 x    =  ih * k.x * rcp_wavenumber;
    float2 y    =  ih * k.y * rcp_wavenumber;
    float2 z    =   h;
    float2 xdx  = - h * k.x * k.x * rcp_wavenumber;
    float2 ydx  = - h * k.y * k.x * rcp_wavenumber;
    float2 zdx  =  ih * k.x;
    float2 ydy  = - h * k.y * k.y * rcp_wavenumber;
    float2 zdy  =  ih * k.y;

    uint3 shifted_pos = uint3((id.xy + uint2(pc.texture_size,pc.texture_size) / 2) % pc.texture_size, id.z);

    x_y_z_xdx_tex.store_2d_array_uniform(      shifted_pos, float4(ren::cadd(  x, ren::cmuli(y)  ), ren::cadd(  z, ren::cmuli(xdx))));
    ydx_zdx_ydy_zdy_tex.store_2d_array_uniform(shifted_pos, float4(ren::cadd(ydx, ren::cmuli(zdx)), ren::cadd(ydy, ren::cmuli(zdy))));
}
