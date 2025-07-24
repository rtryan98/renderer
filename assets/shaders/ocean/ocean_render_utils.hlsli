#ifndef REN_OCEAN_RENDER_UTILS
#define REN_OCEAN_RENDER_UTILS

float heaviside(float x)
{
    if (x >= 0.) return 1.;
    return 0.;
}

float calculate_det_jacobian(float x_dx, float x_dy, float y_dx, float y_dy)
{
    float Jxx = 1. + x_dx;
    float Jxy = x_dy;
    float Jyx = y_dx;
    float Jyy = 1. + y_dy;
    return Jxx * Jyy - Jyx * Jxy;
}

float2 calculate_slope(float z_dx, float z_dy, float x_dx, float y_dy)
{
    return float2(z_dx / (1.0 + /* lambda * */x_dx), z_dy / (1.0 + /* lambda * */y_dy));
}

float3 calculate_normals(float2 slope)
{
    return normalize(float3(-slope.x, -slope.y, 1.0));
}

#endif
