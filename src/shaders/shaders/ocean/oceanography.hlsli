#ifndef REN_OCEANOGRAPHY
#define REN_OCEANOGRAPHY

#include "shaders/constants.hlsli"
#include "shaders/trigonometry.hlsli"
#include "shaders/util.hlsli"

namespace ren
{
namespace ocean
{

static const float SECH_CLAMP = 9.;
static const float CAPILLARY_SIGMA = 0.074;
static const float CAPILLARY_RHO = 1000.;
static const float CAPILLARY_SIGMA_OVER_RHO = CAPILLARY_SIGMA / CAPILLARY_RHO;

namespace detail
{
float clmp_sech(float x)
{
    return ren::sech(clamp(x, -SECH_CLAMP, SECH_CLAMP));
}
}

// Dispersion relations

struct Dispersion_Args
{
    float k;
    float g;
    float h;
};

float dispersion_deep(Dispersion_Args args)
{
    return sqrt(args.g * args.k);
}

float dispersion_deep_ddk(Dispersion_Args args)
{
    return args.g / (2. * sqrt(args.g * args.k));
}

float dispersion_finite_depth(Dispersion_Args args)
{
    return sqrt(args.g * args.k * tanh(args.k * args.h));
}

float dispersion_finite_depth_ddk(Dispersion_Args args)
{
    float kh = args.k * args.h;
    float gk = args.g * args.k;
    return args.g * (tanh(kh) + kh * pow(detail::clmp_sech(kh), 2.)) / (2. * sqrt(gk * tanh(kh)));
}

float dispersion_capillary(Dispersion_Args args)
{
    float kh = args.k * args.h;
    float gk = args.g * args.k;
    return sqrt(gk + CAPILLARY_SIGMA_OVER_RHO * pow(args.k, 3.) * tanh(kh));
}

float dispersion_capillary_ddk(Dispersion_Args args)
{
    float k2 = args.k * args.k;
    float k3 = args.k * k2;
    float gk = args.g * args.k;
    float kh = args.k * args.h;
    float dividend =
        ((3. * CAPILLARY_SIGMA_OVER_RHO * k2 + args.g) * tanh(kh)) +
        (args.h * (CAPILLARY_SIGMA_OVER_RHO * k3 + gk) * pow(detail::clmp_sech(kh), 2.));
    float divisor = 2. * sqrt((CAPILLARY_SIGMA_OVER_RHO * k3 + gk) * tanh(kh));
    return dividend / divisor;
}

// Non-directional wave spectra peak functions

struct Omega_Peak_Args
{
    float omega;
    float u; // wind speed 10m above sea level
    float g; // gravity
    float f; // fetch
};

float omega_peak_pierson_moskowitz(Omega_Peak_Args args)
{
    return (0.855 * args.g) / args.u;
}

float omega_peak_jonswap(Omega_Peak_Args args)
{
    float chi = min(1000, args.g * args.f * 1000. / args.u / args.u);
    float nu = 3.5 * pow(chi, -0.33);
    return TWO_PI * args.g * nu / args.u;
}

// Non-directional wave spectra

struct Spectrum_Args
{
    float omega;
    float omega_peak;
    float u; // wind speed 10m above sea level
    float g; // gravity
    float f; // fetch
    float h; // depth
    float phillips_alpha;
    float generalized_a;
    float generalized_b;
};

float spectrum_phillips(Spectrum_Args args)
{
    return args.phillips_alpha * TWO_PI * (pow(args.g, 2.) / pow(args.omega, 5.));
}

float spectrum_pierson_moskowitz(Spectrum_Args args)
{
    float alpha = 0.0081;
    float beta = 0.74;
    return ((alpha * pow(args.g, 2.))/pow(args.omega, 5.)) * exp(-beta * pow(args.omega_peak / args.omega, 4.));
}

float spectrum_generalized_a_b(Spectrum_Args args)
{
    return (args.generalized_a / pow(args.omega, 5.)) * exp((-args.generalized_b) / pow(args.omega, 4.));
}

float spectrum_jonswap(Spectrum_Args args)
{
    float gamma = 3.3;
    float sigma_0 = 0.07;
    float sigma_1 = 0.09;
    float sigma = args.omega <= args.omega_peak
        ? sigma_0
        : sigma_1;
    // float alpha = 0.076f * pow(pow(u, 2.0f) / (f * g), 0.22f);
    float chi = min(1000., args.g * args.f * 1000. / args.u / args.u);
    float alpha = 0.076 * pow(chi, -0.22);
    float r = exp(-(pow(args.omega - args.omega_peak, 2.) / (2. * pow(sigma, 2.) * pow(args.omega_peak, 2.))));
    return ((alpha * pow(args.g, 2.0f))/(pow(args.omega, 5.))) * exp(-1.25 * pow(args.omega_peak / args.omega, 4.)) * pow(gamma, r);
}

float spectrum_tma(Spectrum_Args args)
{
    float omega_h = args.omega * sqrt(args.h / args.g);
    float phi_0 = .5 * pow(omega_h, 2.);
    float phi_1 = 1. - 0.5f * pow(2. - omega_h, 2.);
    float phi = omega_h <= 1.0
        ? phi_0
        : omega_h <= 2.0
            ? phi_1
            : 1.0;
    return phi * spectrum_jonswap(args);
}

// Directional spreading functions

struct Directional_Spreading_Args
{
    float theta;
    float omega;
    float omega_peak;
    float u;
    float g;
};

namespace detail
{
float mitsuyasu_s(Directional_Spreading_Args args)
{
    float s_p = 11.5f * pow((args.omega_peak * args.u) / args.g, -2.5);
    float s0 = pow(s_p,  5.);
    float s1 = pow(s_p, -2.5);
    return args.omega > args.omega_peak
        ? s1
        : s0;
}

float mitsuyasu_q(float s)
{
    float a = pow(2., 2. * s - 1) / PI;
    float b = pow(stirling_approximation(s + 1), 2.);
    float c = stirling_approximation(2.0 * s + 1);
    return a * (b / c);
}

float hasselmann_s(Directional_Spreading_Args args)
{
    float s0 = 6.97 * pow(args.omega / args.omega_peak, 4.06);
    float s1_exp = -2.33 - 1.45 * ((args.u * args.omega_peak / args.g) - 1.17);
    float s1 = 9.77 * pow(args.omega / args.omega_peak, s1_exp);
    return args.omega > args.omega_peak
        ? s1
        : s0;
}

float donelan_banner_beta_s(float omega, float omega_peak)
{
    float om_over_omp = omega / omega_peak;
    float epsilon = -0.4 + 0.8393 * exp(-0.567 * pow(log(om_over_omp), 2.));
    float beta_s_0 = 2.61 * pow(om_over_omp, 1.3);
    float beta_s_1 = 2.28 * pow(om_over_omp, -1.3);
    float beta_s_2 = pow(10.0, epsilon);
    return om_over_omp < 0.95
        ? beta_s_0
        : om_over_omp < 1.6
            ? beta_s_1
            : beta_s_2;
    // Clamping om_over_omp between 0.54 and 0.95 is correct according to [Donelan et al. 1985],
    // however, cutting off om_over_omp at 0.54 and returning 0.0 instead
    // will result in a less pleasing result according to [Horvath 2015].
}
}

float dirspread_pos_cos_sq(Directional_Spreading_Args args)
{
    float a = 2. / PI * pow(cos(args.theta), 2.);
    bool cond = (-PI / 2. < args.theta) && (args.theta < PI / 2.);
    return cond ? a : 0.;
}

float dirspread_mitsuyasu(Directional_Spreading_Args args)
{
    float s = detail::mitsuyasu_s(args);
    float q_s = detail::mitsuyasu_q(s);
    return q_s * pow(abs(cos(args.theta / 2.)), 2. * s);
}

float dirspread_hasselmann(Directional_Spreading_Args args)
{
    float s = detail::hasselmann_s(args);
    float q_s = detail::mitsuyasu_q(s);
    return q_s * pow(abs(cos(args.theta / 2.)), 2. * s);
}

float dirspread_donelan_banner(Directional_Spreading_Args args)
{
    float beta_s = detail::donelan_banner_beta_s(args.omega, args.omega_peak);
    return (beta_s/(2. * tanh(beta_s * PI))) * pow(detail::clmp_sech(beta_s * args.theta), 2.);
}

float dirspread_flat()
{
    return 1. / TWO_PI;
}

}
}

#endif
