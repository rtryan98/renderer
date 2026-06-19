#ifndef REN_ERF_HLSLI
#define REN_ERF_HLSLI

namespace ren
{
// Abramowitz and Stegun
float erf_approx(float x)
{
    const float flip = sign(x);
    x = abs(x);
    static const float p = 0.3275911;
    static const float a1 =  0.254829592;
    static const float a2 = -0.284496736;
    static const float a3 =  1.421413741;
    static const float a4 = -1.453152027;
    static const float a5 =  1.061405429;
    const float t = rcp(1. + p*x);
    const float t2 = t*t;
    const float t3 = t2*t;
    const float t4 = t3*t;
    const float t5 = t4*t;
    return flip * (1. - (a1*t + a2*t2 + a3*t3 + a4*t4 + a5*t5) * exp(-(x*x)));
}

float erfc_approx(float x)
{
    return 1. - erf_approx(x);
}
}

#endif
