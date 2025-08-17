#ifndef REN_COLOR_TF
#define REN_COLOR_TF

namespace ren
{
namespace color
{
namespace transfer_functions
{
// sRGB explanation:
// C_l: the linear signal, where C in [R, G, B] and in range [0.0, 1.0].
// C_n: the non-linear signal.

template<typename T>
T EOTF_sRGB(T C_n_Rec709)
{
    T lower = C_n_Rec709 / 12.92;
    T upper = pow((C_n_Rec709 + 0.055) / 1.055, 2.4);
    return select(C_n_Rec709 <= 0.04045, lower, upper);
}

template<typename T>
T IEOTF_sRGB(T C_l_Rec709)
{
    T lower = C_l_Rec709 * 12.92;
    T upper = 1.055 * pow(C_l_Rec709, rcp(2.4)) - 0.055;
    return select(C_l_Rec709 <= 0.0031308, lower, upper);
}

// gamma explanation:
//   C_l: the linear signal, where C in [R, G, B] and in range [0.0, 1.0].
//   c_n: the non-linear signal
// gamma: the exponent used for compression; Typically 2.2. For BT1886 use 2.4 and scale EOTF output by 100.0 and IEOTF input by 1.0 / 100.0;

template<typename T, typename U>
T EOTF_gamma(T C_n_Rec709, U gamma)
{
    return pow(C_n_Rec709, gamma);
}

template<typename T, typename U>
T IEOTF_gamma(T C_l_Rec709, U gamma)
{
    return pow(C_l_Rec709, rcp(gamma));
}

// PQ explanation:
//   E: the non-linear signal in range [0.0, 1.0].
// F_D: the displayed luminance in nits, in range [0.0, 10000.0].
//   Y: the normalized linear displayed signal, in range [0.0, 1.0], where Y = F_D / 10000.0.
// The scaled functions rescale the signal provided a peak luminance value, usually the limit of the display.
// When using the scaled functions, the input F_D must be clamped in the range [0.0, max_nits].
// Exponential scale factor with a value different from 1.0 is required for Jzazbz color space.

static const float PQ_m1 = 0.1593017578125;
static const float PQ_m2 = 78.84375;
static const float PQ_c3 = 18.6875;
static const float PQ_c2 = 18.8515625;
static const float PQ_c1 = PQ_c3 - PQ_c2 + 1.;

template<typename T>
T EOTF_PQ(T E_Rec2020, float exponential_scale_factor = 1.0)
{
    E_Rec2020 = saturate(E_Rec2020);
    T E1m2 = pow(E_Rec2020, rcp(PQ_m2 * exponential_scale_factor));
    return 10000.0 * pow(max(E1m2 - PQ_c1, 0.0) / (PQ_c2 - PQ_c3 * E1m2), rcp(PQ_m1));
}

template<typename T>
T IEOTF_PQ(T F_D_Rec2020, float exponential_scale_factor = 1.0)
{
    F_D_Rec2020 = clamp(F_D_Rec2020, 0.0, 10000.0);
    T Ym1 = pow(F_D_Rec2020 / 10000.0, PQ_m1);
    return pow((PQ_c1 + PQ_c2 * Ym1) / (1.0 + PQ_c3 * Ym1), PQ_m2 * exponential_scale_factor);
}

template<typename T, typename U>
T EOTF_PQ_scaled(T E_Rec2020, U max_nits, float exponential_scale_factor = 1.0)
{
    E_Rec2020 = saturate(E_Rec2020);
    T E1m2 = pow(E_Rec2020, rcp(PQ_m2 * exponential_scale_factor));
    return (10000.0 / max_nits) * pow(max(E1m2 - PQ_c1, 0.0) / (PQ_c2 - PQ_c3 * E1m2), rcp(PQ_m1));
}

template<typename T, typename U>
T IEOTF_PQ_scaled(T F_D_Rec2020, U max_nits, float exponential_scale_factor = 1.0)
{
    F_D_Rec2020 = clamp(F_D_Rec2020, 0.0, 10000.0);
    T Ym1 = pow(F_D_Rec2020 * max_nits / 10000.0, PQ_m1);
    return pow((PQ_c1 + PQ_c2 * Ym1) / (1.0 + PQ_c3 * Ym1), PQ_m2 * exponential_scale_factor);
}

} // namespace transfer_functions
} // namespace color
} // namespace ren

#endif
