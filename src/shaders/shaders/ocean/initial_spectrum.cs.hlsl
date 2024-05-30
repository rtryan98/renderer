#include "shaders/ocean/oceanography.hlsli"
#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/rand.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Initial_Spectrum_Push_Constants, pc);

float pcgnoise(int3 a)
{
    return ren::box_muller_12(1.0/float(0xffffffffu) * float2(ren::pcg3d(a).xy));
}

float calculate_spectrum(float wavenumber, float delta_k, float theta, float g, Ocean_Spectrum_Data spectrum_data)
{
    ren::ocean::Dispersion_Args dispersion_args = { wavenumber, g, spectrum_data.f };
    float omega = ren::ocean::dispersion_capillary(dispersion_args);
    float omega_ddk = ren::ocean::dispersion_capillary_ddk(dispersion_args);
    float wind_direction = spectrum_data.wind_direction / 180. * ren::PI;

    ren::ocean::Omega_Peak_Args omega_peak_args = {
        omega,
        spectrum_data.u,
        g,
        spectrum_data.f,
        spectrum_data.phillips_alpha
    };
    float omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args);

    ren::ocean::Spectrum_Args spectrum_args = {
        omega,
        omega_peak,
        spectrum_data.u,
        g,
        spectrum_data.f,
        spectrum_data.h,
        spectrum_data.phillips_alpha,
        spectrum_data.generalized_a,
        spectrum_data.generalized_b
    };
    float spectrum = ren::ocean::spectrum_tma(spectrum_args);
    spectrum = sqrt(2. * spectrum * abs(omega_ddk / wavenumber) * delta_k * delta_k);

    ren::ocean::Directional_Spreading_Args directional_spreading_args = {
        theta - wind_direction,
        omega,
        omega_peak,
        spectrum_data.u,
        g
    };
    float directional_spread = ren::ocean::dirspread_hasselmann(directional_spreading_args);

    return spectrum_data.contribution * directional_spread * spectrum;
}

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    rhi::RW_Texture spectrum_tex = { pc.spectrum_tex };

    float u = 4.;
    float g = 9.81;
    float f = 1000.;
    float h = 30.;
    float phillips_alpha = 1.;
    float generalized_a = 1.;
    float generalized_b = 1.;

    int2 id_shifted = int2(id.xy) - int2(256, 256) / 2;
    float delta_k = ren::TWO_PI / 1.;
    float2 k = id_shifted * delta_k;
    float wavenumber = length(k);
    float theta = atan2(k.y, k.x);

    ren::ocean::Dispersion_Args dispersion_args = { wavenumber, g, f };
    float omega = ren::ocean::dispersion_capillary(dispersion_args);
    float omega_ddk = ren::ocean::dispersion_capillary_ddk(dispersion_args);

    ren::ocean::Omega_Peak_Args omega_peak_args = { omega, u, g, f, phillips_alpha };
    float omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args);

    ren::ocean::Spectrum_Args spectrum_args = { omega, omega_peak, u, g, f, h, phillips_alpha, generalized_a, generalized_b };
    float spectrum = ren::ocean::spectrum_tma(spectrum_args);
    spectrum = sqrt(2. * spectrum * abs(omega_ddk / wavenumber) * delta_k * delta_k);

    ren::ocean::Directional_Spreading_Args directional_spreading_args = { theta, omega, omega_peak, u, g };
    float directional_spread = ren::ocean::dirspread_hasselmann(directional_spreading_args);

    float2 noise = float2(pcgnoise(id), pcgnoise(id + uint3(0,0,4)));
    float2 directional_spectrum = 1. / sqrt(2.) * noise * directional_spread * spectrum;

    spectrum_tex.store_2d_array_uniform(id, float4(directional_spectrum, k));
}
