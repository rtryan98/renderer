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
    static const float EPSILON = 1.0/float(0xffffffffu);
    float nx = z_dx;
    float ny = z_dy;

    float nx_dx = 1.0 + x_dx;
    float ny_dy = 1.0 + y_dy;

    if (nx_dx < 0.0)
    {
        nx_dx = -nx_dx;
    }
    if (ny_dy < 0.0)
    {
        ny_dy = -ny_dy;
    }

    if (abs(nx_dx) > 0.0)
    {
        nx /= nx_dx;
    }
    else
    {
        nx = 0.0;
    }
    if (abs(ny_dy) > 0.0)
    {
        ny /= ny_dy;
    }
    else
    {
        ny = 0.0;
    }

    return float2(nx, ny);
}

float3 calculate_normals(float2 slope)
{
    return normalize(float3(-slope.x, -slope.y, 1.0));
}

float4 calculate_cascade_sampling_weights(float distance, float distance_factor, float lod_lengthscale_factor, float4 lengthscales)
{
    return 1.0 - smoothstep(0.0, lod_lengthscale_factor * lengthscales, distance_factor * distance);
}

#endif
