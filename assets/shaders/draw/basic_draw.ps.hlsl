#include "draw/basic_draw.hlsli"

PS_Out main(PS_In ps_in)
{
    // PS_Out result = { float4(ps_in.tex_coord.x, ps_in.tex_coord.y, 0.0, 1.0 ) };
    // PS_Out result = { 0.5 + 0.5 * float4(ps_in.normal.x, ps_in.normal.y, ps_in.normal.z, 0.0) };
    PS_Out result = { 0.5 + 0.5 * float4(ps_in.tangent.x, ps_in.tangent.y, ps_in.tangent.z, 0.0) };
    // PS_Out result = { float4(1., 0.5, 0., 1.) };
    return result;
}
