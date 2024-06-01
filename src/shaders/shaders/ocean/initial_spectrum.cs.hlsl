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
    };
    float omega_peak = 0.;
    switch (spectrum_data.spectrum)
    {
        case Pierson_Moskowitz: omega_peak = ren::ocean::omega_peak_pierson_moskowitz(omega_peak_args); break;
        case Jonswap:           omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args); break;
        case TMA:               omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args); break;
        default: break;
    }

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
    float spectrum = 0.;
    switch (spectrum_data.spectrum)
    {
        case Phillips:          spectrum = ren::ocean::spectrum_phillips(spectrum_args); break;
        case Pierson_Moskowitz: spectrum = ren::ocean::spectrum_pierson_moskowitz(spectrum_args); break;
        case Generalized_A_B:   spectrum = ren::ocean::spectrum_generalized_a_b(spectrum_args); break;
        case Jonswap:           spectrum = ren::ocean::spectrum_jonswap(spectrum_args); break;
        case TMA:               spectrum = ren::ocean::spectrum_tma(spectrum_args); break;
    }
    spectrum = sqrt(2. * spectrum * abs(omega_ddk / wavenumber) * delta_k * delta_k);

    ren::ocean::Directional_Spreading_Args directional_spreading_args = {
        theta - wind_direction,
        omega,
        omega_peak,
        spectrum_data.u,
        g
    };
    float directional_spread = 0.;
    switch (spectrum_data.directional_spreading_function)
    {
        case Positive_Cosine_Squared: directional_spread = ren::ocean::dirspread_pos_cos_sq(directional_spreading_args);     break;
        case Mitsuyasu:               directional_spread = ren::ocean::dirspread_mitsuyasu(directional_spreading_args);      break;
        case Hasselmann:              directional_spread = ren::ocean::dirspread_hasselmann(directional_spreading_args);     break;
        case Donelan_Banner:          directional_spread = ren::ocean::dirspread_donelan_banner(directional_spreading_args); break;
        case Flat:                    directional_spread = ren::ocean::dirspread_flat();                                     break;
    }

    return spectrum_data.contribution * directional_spread * spectrum;
}

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    rhi::RW_Texture spectrum_tex = { pc.spectrum_tex };
    rhi::Raw_Buffer initial_spectrum_buffer = { pc.data };
    Ocean_Initial_Spectrum_Data data = initial_spectrum_buffer.load<Ocean_Initial_Spectrum_Data>();

    int2 id_shifted = int2(id.xy) - int2(data.texture_size, data.texture_size) / 2;
    float delta_k = ren::TWO_PI / data.length_scales[id.z];
    float2 k = id_shifted * delta_k;
    float wavenumber = length(k);
    float theta = atan2(k.y, k.x);

    float spectra[2] = {
        calculate_spectrum(wavenumber, delta_k, theta, data.g, data.spectra[0]),
        calculate_spectrum(wavenumber, delta_k, theta, data.g, data.spectra[1])
    };
    float spectrum = spectra[0] + spectra[1];

    float2 noise = float2(pcgnoise(id), pcgnoise(id + uint3(0,0,4)));
    float2 directional_spectrum = 1. / sqrt(2.) * noise * spectrum * float(data.active_cascades[id.z]);

    spectrum_tex.store_2d_array_uniform(id, float4(directional_spectrum, k));
}
