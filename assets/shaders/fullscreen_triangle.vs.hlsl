// SHADER DEF fullscreen_triangle
// ENTRYPOINT main
// TYPE vs
// SHADER END DEF

struct VS_Out
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VS_Out main(uint vertex_id : SV_VertexID)
{
    float2 uv = float2((vertex_id << 1) & 0x2, vertex_id & 0x2);
    VS_Out result = { float4(uv * float2(2., -2.) + float2(-1., 1.), 0., 1.), uv };
    return result;
}
