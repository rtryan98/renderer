#include "shaders/ocean/oceanography.hlsli"
#include "shared/ocean_shared_types.h"
#include "rhi/bindless.hlsli"
#include "shaders/rand.hlsli"

DECLARE_PUSH_CONSTANTS(Ocean_Initial_Spectrum_Push_Constants, pc);

static const float MAX_LENGTHSCALE = 100000.;

float pcgnoise(int3 a)
{
    return ren::box_muller_12(1.0/float(0xffffffffu) * float2(ren::pcg3d(a).xy));
}

float2 calculate_dispersion_and_derivative(float wavenumber, float g, float h)
{
    ren::ocean::Dispersion_Args dispersion_args = { wavenumber, g, h };
    float omega = ren::ocean::dispersion_finite_depth(dispersion_args);
    float omega_ddk = ren::ocean::dispersion_finite_depth_ddk(dispersion_args);
    return float2(omega, omega_ddk);
}

float calculate_peak_angular_frequency(float omega, float u, float g, float f, uint oceanographic_spectrum)
{
    ren::ocean::Omega_Peak_Args omega_peak_args = {
        omega,
        u,
        g,
        f,
    };
    float omega_peak = 0.;
    switch (oceanographic_spectrum)
    {
        case Pierson_Moskowitz: omega_peak = ren::ocean::omega_peak_pierson_moskowitz(omega_peak_args); break;
        case Jonswap:           omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args); break;
        case TMA:               omega_peak = ren::ocean::omega_peak_jonswap(omega_peak_args); break;
        default: break;
    }
    return omega_peak;
}

float calculate_nondirectional_spectrum(float omega, float omega_peak, float u, float g, float f, float h, float phillips_alpha, float generalized_a, float generalized_b, uint oceanographic_spectrum)
{
    ren::ocean::Spectrum_Args spectrum_args = {
        omega,
        omega_peak,
        u,
        g,
        f,
        h,
        phillips_alpha,
        generalized_a,
        generalized_b
    };
    float spectrum = 0.;
    switch (oceanographic_spectrum)
    {
        case Phillips:          spectrum = ren::ocean::spectrum_phillips(spectrum_args); break;
        case Pierson_Moskowitz: spectrum = ren::ocean::spectrum_pierson_moskowitz(spectrum_args); break;
        case Generalized_A_B:   spectrum = ren::ocean::spectrum_generalized_a_b(spectrum_args); break;
        case Jonswap:           spectrum = ren::ocean::spectrum_jonswap(spectrum_args); break;
        case TMA:               spectrum = ren::ocean::spectrum_tma(spectrum_args); break;
    }
    return spectrum;
}

float calculate_directional_spread(float theta, float omega, float omega_peak, float u, float g, uint directional_spreading_function)
{
    ren::ocean::Directional_Spreading_Args directional_spreading_args = {
        theta,
        omega,
        omega_peak,
        u,
        g
    };
    float directional_spread = 0.;
    switch (directional_spreading_function)
    {
        case Positive_Cosine_Squared: directional_spread = ren::ocean::dirspread_pos_cos_sq(directional_spreading_args);     break;
        case Mitsuyasu:               directional_spread = ren::ocean::dirspread_mitsuyasu(directional_spreading_args);      break;
        case Hasselmann:              directional_spread = ren::ocean::dirspread_hasselmann(directional_spreading_args);     break;
        case Donelan_Banner:          directional_spread = ren::ocean::dirspread_donelan_banner(directional_spreading_args); break;
        case Flat:                    directional_spread = ren::ocean::dirspread_flat();                                     break;
    }
    return directional_spread;
}

float calculate_spectrum(float2 dispersion_and_derivative, float delta_k, float theta, float g, float h, uint oceanographic_spectrum, uint directional_spreading_function, Ocean_Spectrum_Data spectrum_data)
{
    float omega = dispersion_and_derivative[0];
    float omega_ddk = dispersion_and_derivative[1];

    float omega_peak = calculate_peak_angular_frequency(omega, spectrum_data.u, g, spectrum_data.f, oceanographic_spectrum);
    
    float spectrum = calculate_nondirectional_spectrum(omega, omega_peak, spectrum_data.u, g, spectrum_data.f, h, spectrum_data.phillips_alpha, spectrum_data.generalized_a, spectrum_data.generalized_b, oceanographic_spectrum);
    float wind_direction = spectrum_data.wind_direction / 180. * ren::PI;
    float directional_spread = calculate_directional_spread(theta - wind_direction, omega, omega_peak, spectrum_data.u, g, directional_spreading_function);
    spectrum = spectrum_data.contribution * directional_spread * spectrum;

    return spectrum;
}

float calculate_min_wavenumber(float length_scale)
{
    return sqrt(2.) * ren::TWO_PI / length_scale;
}

float calculate_max_wavenumber(float length_scale, float texture_size)
{
    return ren::PI * texture_size / length_scale;
}

[numthreads(32, 32, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    rhi::RW_Texture spectrum_tex = { pc.spectrum_tex };
    rhi::RW_Texture angular_frequency_tex = { pc.angular_frequency_tex };
    rhi::Raw_Buffer initial_spectrum_buffer = { pc.data };
    Ocean_Initial_Spectrum_Data data = initial_spectrum_buffer.load<Ocean_Initial_Spectrum_Data>();

    int2 id_shifted = int2(id.xy) - int2(data.texture_size, data.texture_size) / 2;
    float delta_k = ren::TWO_PI / data.length_scales[id.z];
    float2 k = id_shifted * delta_k;
    float wavenumber = length(k);
    float theta = atan2(k.y, k.x);

    float2 dispersion = calculate_dispersion_and_derivative(wavenumber, data.g, data.h);
    float omega = dispersion[0];
    float omega_ddk = dispersion[1];

    float2 spectra = float2(
        calculate_spectrum(dispersion, delta_k, theta, data.g, data.h, data.spectrum, data.directional_spreading_function, data.spectra[0]),
        calculate_spectrum(dispersion, delta_k, theta, data.g, data.h, data.spectrum, data.directional_spreading_function, data.spectra[1]));
    float spectrum = sqrt(2. * (spectra[0] + spectra[1]) * abs(omega_ddk / wavenumber) * delta_k * delta_k);

    float2 noise = float2(pcgnoise(id), pcgnoise(id + uint3(0,0,4)));
    float2 initial_spectral_state = rcp(sqrt(2)) * noise * spectrum * float(data.active_cascades[id.z]);

    float min_wavenumber = calculate_min_wavenumber(data.length_scales[id.z]);
    float max_wavenumber = calculate_max_wavenumber(data.length_scales[id.z], data.texture_size);
    float max_wavenumber_prev_cascade = id.z <= 0 ? 0.f : calculate_max_wavenumber(data.length_scales[id.z - 1], data.texture_size);
    bool overlap_low = wavenumber > max_wavenumber_prev_cascade;
    if (wavenumber < min_wavenumber || wavenumber > max_wavenumber || overlap_low)
    {
        initial_spectral_state = float2(0.,0.);
        omega = 0.0;
    }

    spectrum_tex.store_2d_array_uniform(id, float4(initial_spectral_state, k));
    angular_frequency_tex.store_2d_array_uniform(id, omega);
}
