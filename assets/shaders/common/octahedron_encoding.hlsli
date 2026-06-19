#ifndef REN_OCTAHEDRON_ENCODING
#define REN_OCTAHEDRON_ENCODING

#include "constants.hlsli"

namespace ren
{
// https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
float3 oct_signed_encode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float3 on;
    on.y = n.y * .5 + .5;
    on.x = n.x * .5 + on.y;
    on.y = n.x * -.5 + on.y;
    on.z = saturate(n.z * FLOAT_MAX);
    return on;
}

float3 oct_signed_decode(float3 n)
{
    float3 on;
    on.x = (n.x-n.y);
    on.y = (n.x+n.y)-1.;
    on.z = n.z*2.-1.;
    on.z = on.z*(1.-abs(on.x)-abs(on.y));
    return normalize(on);
}
} // namespace ren

#endif
