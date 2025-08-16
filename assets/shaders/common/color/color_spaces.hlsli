#ifndef REN_COLOR_SPACES
#define REN_COLOR_SPACES

#include "common/color/transfer_functions.hlsli"

namespace ren
{
namespace color
{
namespace spaces
{
// Conversions are given in the form <Source Color Space>_<Destination Color Space>
// The primaries and white points for the RGB color spaces are described with the given CIE xy values:
// Rec709:  R: (0.6400, 0.3300), G: (0.3000, 0.6000), B: (0.1500,  0.0600), W: (0.31270, 0.32900)
// Rec2020: R: (0.7080, 0.2920), G: (0.1700, 0.7970), B: (0.1310,  0.0460), W: (0.31270, 0.32900)
// P3D65:   R: (0.6800, 0.3200), G: (0.2650, 0.6900), B: (0.1500,  0.0600), W: (0.31270, 0.32900)
// AP0:     R: (0.7347, 0.2653), G: (0.0000, 1.0000), B: (0.0001, -0.0770), W: (0.32168, 0.33767)
// AP1:     R: (0.7130, 0.2930), G: (0.1650, 0.8300), B: (0.1280,  0.0440), W: (0.32168, 0.33767)
// Additionally, Conversions between Rec2020 and the color spaces ICtCp and Jzazbz are provided.
// Licence notice: Rec2020 <-> ICtCp and Rec2020 <-> Jzazbz conversions taken roughly from GT7 Tone Mapping, licenced under MIT.
// https://blog.selfshadow.com/publications/s2025-shading-course/pdi/supplemental/gt7_tone_mapping.cpp

static const float3x3 MAT_Rec709_XYZ = {
     0.412391,  0.357584,  0.180481,
     0.212639,  0.715169,  0.072192,
     0.019331,  0.119195,  0.950532
};

static const float3x3 MAT_Rec2020_XYZ = {
     0.636958,  0.144617,  0.168881,
     0.262700,  0.677998,  0.059302,
     0.000000,  0.028073,  1.060985
};

static const float3x3 MAT_P3D65_XYZ = {
     0.486571,  0.265668,  0.198217,
     0.228975,  0.691739,  0.079287,
     0.000000,  0.045113,  1.043944
};

static const float3x3 MAT_AP0_XYZ = {
     0.952552,  0.000000,  0.000094,
     0.343966,  0.728166, -0.072133,
     0.000000,  0.000000,  1.008825
};

static const float3x3 MAT_AP1_XYZ = {
     0.662454,  0.134004,  0.156188,
     0.272229,  0.674082,  0.053690,
    -0.005575,  0.004061,  1.010339
};

static const float3x3 MAT_XYZ_Rec709 = {
     3.240970, -1.537383, -0.498611,
    -0.969244,  1.875968,  0.041555,
     0.055630, -0.203977,  1.056972
};

static const float3x3 MAT_XYZ_Rec2020 = {
     1.716651, -0.355671, -0.253366,
    -0.666684,  1.616481,  0.015769,
     0.017640, -0.042771,  0.942103
};

static const float3x3 MAT_XYZ_P3D65 = {
     2.493497, -0.931384, -0.402711,
    -0.829489,  1.762664,  0.023625,
     0.035846, -0.076172,  0.956885
};

static const float3x3 MAT_XYZ_AP0 = {
     1.049811,  0.000000, -0.000097,
    -0.495903,  1.373313,  0.098240,
     0.000000,  0.000000,  0.991252
};

static const float3x3 MAT_XYZ_AP1 = {
     1.641023, -0.324803, -0.236425,
    -0.663663,  1.615332,  0.016756,
     0.011722, -0.008284,  0.988395
};

static const float3x3 MAT_Rec709_Rec2020 = mul(MAT_XYZ_Rec2020, MAT_XYZ_Rec709);
static const float3x3 MAT_Rec709_P3D65   = mul(MAT_XYZ_P3D65  , MAT_XYZ_Rec709);
static const float3x3 MAT_Rec709_AP0     = mul(MAT_XYZ_AP0    , MAT_XYZ_Rec709);
static const float3x3 MAT_Rec709_AP1     = mul(MAT_XYZ_AP1    , MAT_XYZ_Rec709);

static const float3x3 MAT_Rec2020_Rec709 = mul(MAT_XYZ_Rec709, MAT_Rec2020_XYZ);
static const float3x3 MAT_Rec2020_P3D65  = mul(MAT_XYZ_P3D65 , MAT_Rec2020_XYZ);
static const float3x3 MAT_Rec2020_AP0    = mul(MAT_XYZ_AP0   , MAT_Rec2020_XYZ);
static const float3x3 MAT_Rec2020_AP1    = mul(MAT_XYZ_AP1   , MAT_Rec2020_XYZ);

static const float3x3 MAT_P3D65_Rec709   = mul(MAT_XYZ_Rec709 , MAT_P3D65_XYZ);
static const float3x3 MAT_P3D65_Rec2020  = mul(MAT_XYZ_Rec2020, MAT_P3D65_XYZ);
static const float3x3 MAT_P3D65_AP0      = mul(MAT_XYZ_AP0    , MAT_P3D65_XYZ);
static const float3x3 MAT_P3D65_AP1      = mul(MAT_XYZ_AP1    , MAT_P3D65_XYZ);

static const float3x3 MAT_AP0_Rec709     = mul(MAT_XYZ_Rec709 , MAT_AP0_XYZ);
static const float3x3 MAT_AP0_Rec2020    = mul(MAT_XYZ_Rec2020, MAT_AP0_XYZ);
static const float3x3 MAT_AP0_P3D65      = mul(MAT_XYZ_P3D65  , MAT_AP0_XYZ);
static const float3x3 MAT_AP0_AP1        = mul(MAT_XYZ_AP1    , MAT_AP0_XYZ);

static const float3x3 MAT_AP1_Rec709     = mul(MAT_XYZ_Rec709 , MAT_AP1_XYZ);
static const float3x3 MAT_AP1_Rec2020    = mul(MAT_XYZ_Rec2020, MAT_AP1_XYZ);
static const float3x3 MAT_AP1_P3D65      = mul(MAT_XYZ_P3D65  , MAT_AP1_XYZ);
static const float3x3 MAT_AP1_AP0        = mul(MAT_XYZ_AP0    , MAT_AP1_XYZ);


// CIE XYZ conversions

float3 XYZ_Rec709(float3 XYZ)
{
    return mul(XYZ, MAT_XYZ_Rec709);
}

float3 XYZ_Rec2020(float3 XYZ)
{
    return mul(XYZ, MAT_XYZ_Rec2020);
}

float3 XYZ_P3D65(float3 XYZ)
{
    return mul(XYZ, MAT_XYZ_P3D65);
}

float3 XYZ_AP0(float3 XYZ)
{
    return mul(XYZ, MAT_XYZ_AP0);
}

float3 XYZ_AP1(float3 XYZ)
{
    return mul(XYZ, MAT_AP0_AP1);
}


// Rec. 709 conversions

float3 Rec709_XYZ(float3 Rec709)
{
    return mul(Rec709, MAT_Rec709_XYZ);
}

float3 Rec709_Rec2020(float3 Rec709)
{
    return mul(Rec709, MAT_Rec709_Rec2020);
}

float3 Rec709_P3D65(float3 Rec709)
{
    return mul(Rec709, MAT_Rec709_P3D65);
}

float3 Rec709_AP0(float3 Rec709)
{
    return mul(Rec709, MAT_Rec709_AP0);
}

float3 Rec709_AP1(float3 Rec709)
{
    return mul(Rec709, MAT_Rec709_AP1);
}


// Rec. 2020 conversions

float3 Rec2020_XYZ(float3 Rec2020)
{
    return mul(Rec2020, MAT_Rec2020_XYZ);
}

float3 Rec2020_Rec709(float3 Rec2020)
{
    return mul(Rec2020, MAT_Rec2020_Rec709);
}

float3 Rec2020_P3D65(float3 Rec2020)
{
    return mul(Rec2020, MAT_Rec2020_P3D65);
}

float3 Rec2020_AP0(float3 Rec2020)
{
    return mul(Rec2020, MAT_Rec2020_AP0);
}

float3 Rec2020_AP1(float3 Rec2020)
{
    return mul(Rec2020, MAT_Rec2020_AP1);
}

float3 Rec2020_ICtCp(float3 Rec2020)
{
    // As described in Dolby's ICtCp whitepaper.
    static const float3 L_ = rcp(4096.) * float3(1688.0, 2146.0, 262.0);
    static const float3 M_ = rcp(4096.) * float3(683.0, 2951.0, 462.0);
    static const float3 S_ = rcp(4096.) * float3(99.0, 309.0, 3688.0);
    float3 LMS = ren::color::transfer_functions::IEOTF_PQ(float3(dot(L_, Rec2020), dot(M_, Rec2020), dot(S_, Rec2020)));
    static const float3 I_ = float3(0.5, 0.5, 0.0);
    static const float3 CT_ = rcp(4096.) * float3(6610.0, -13613.0, 7003.0);
    static const float3 CP_ = rcp(4096.) * float3(17933.0, -17390.0, -543.0);
    return float3(dot(I_, LMS), dot(CT_, LMS), dot(CP_, LMS));
}

float3 Rec2020_Jzazbz(float3 Rec2020)
{
    static const float3 L_ = float3(0.530004, 0.3557, 0.086090);
    static const float3 M_ = float3(0.289388, 0.525395, 0.157481);
    static const float3 S_ = float3(0.091098, 0.147588, 0.734234);
    float3 LMS = ren::color::transfer_functions::IEOTF_PQ(float3(dot(L_, Rec2020), dot(M_, Rec2020), dot(S_, Rec2020)), 1.7);
    float Iz = 0.5 * LMS.x * LMS.y;
    static const float3 A_ = float3(3.524, -4.066708, 0.542708);
    static const float3 B_ = float3(0.199076, 1.096799, -1.295875);
    return float3((0.44 * Iz) / (1.0 - 0.56 * Iz) - 1.6295499532821566e-11, dot(A_, LMS.y), dot(B_, LMS.z));
}


// P3/D65 conversions

float3 P3D65_XYZ(float3 P3D65)
{
    return mul(P3D65, MAT_P3D65_XYZ);
}

float3 P3D65_Rec709(float3 P3D65)
{
    return mul(P3D65, MAT_P3D65_Rec709);
}

float3 P3D65_Rec2020(float3 P3D65)
{
    return mul(P3D65, MAT_P3D65_Rec2020);
}

float3 P3D65_AP0(float3 P3D65)
{
    return mul(P3D65, MAT_P3D65_AP0);
}

float3 P3D65_AP1(float3 P3D65)
{
    return mul(P3D65, MAT_P3D65_AP1);
}


// ACES AP0 conversions

float3 AP0_XYZ(float3 AP0)
{
    return mul(AP0, MAT_AP0_XYZ);
}

float3 AP0_Rec709(float3 AP0)
{
    return mul(AP0, MAT_AP0_Rec709);
}

float3 AP0_Rec2020(float3 AP0)
{
    return mul(AP0, MAT_AP0_Rec2020);
}

float3 AP0_P3D65(float3 AP0)
{
    return mul(AP0, MAT_AP0_P3D65);
}

float3 AP0_AP1(float3 AP0)
{
    return mul(AP0, MAT_AP0_AP1);
}


// ACES AP1 conversions

float3 AP1_XYZ(float3 AP1)
{
    return mul(AP1, MAT_AP1_XYZ);
}

float3 AP1_Rec709(float3 AP1)
{
    return mul(AP1, MAT_AP1_Rec709);
}

float3 AP1_Rec2020(float3 AP1)
{
    return mul(AP1, MAT_AP1_Rec2020);
}

float3 AP1_P3D65(float3 AP1)
{
    return mul(AP1, MAT_AP1_P3D65);
}

float3 AP1_AP0(float3 AP1)
{
    return mul(AP1, MAT_AP1_AP0);
}


// ICtCp conversions

float3 ICtCp_Rec2020(float3 ICtCp)
{
    static const float3 I_ = float3(1.0, 0.00860904, 0.11103);
    static const float3 CT_ = float3(1.0, -0.00860904, -0.11103);
    static const float3 CP_ = float3(1.0, 0.560031, -0.320627);
    float3 LMS = ren::color::transfer_functions::EOTF_PQ(float3(dot(I_, ICtCp), dot(CT_, ICtCp), dot(CP_, ICtCp)));
    static const float3 L_ = float3(3.43661, -2.50645, 0.0698454);
    static const float3 M_ = float3(-0.079133, 1.9836, -0.192271);
    static const float3 S_ = float3(-0.0259499, -0.0989137, 1.12486);
    return max(float3(dot(L_, LMS), dot(M_, LMS), dot(S_, LMS)), 0.0);
}


// Jzazbz conversions

float3 Jzazbz_Rec2020(float3 Jzazbz)
{
    float Jz = Jzazbz.x + 1.6295499532821566e-11;
    Jzazbz.x = Jz / (0.44 + 0.56 * Jz);
    static const float3 Jz_ = float3(1.0, 1.386050432715393e-1, 5.804731615611869e-2);
    static const float3 az_ = float3(1.0, -1.386050432715393e-1, -5.804731615611869e-2);
    static const float3 bz_ = float3(1.0, -9.601924202631895e-2, -8.118918960560390e-1);
    float3 LMS = ren::color::transfer_functions::EOTF_PQ(float3(dot(Jz_, Jzazbz), dot(az_, Jzazbz), dot(bz_, Jzazbz)), 1.7);
    static const float3 L_ = float3(2.990669, -2.049742, 0.088977);
    static const float3 M_ = float3(-1.634525, 3.145627, -0.483037);
    static const float3 S_ = float3(-0.042505, -0.377983, 1.448019);
    return float3(dot(L_, LMS), dot(M_, LMS), dot(S_, LMS));
}

} // namespace spaces
} // namespace color
} // namespace ren

#endif
