// SHADER DEF simple_ray_tracing
// ENTRYPOINT main
// TYPE rchit
// SHADER END DEF

struct [raypayload] Simple_Payload
{
    float4 color : write(closesthit, miss, caller) : read(caller);
};

[shader("closesthit")]
void main(inout Simple_Payload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    float2 bary = attributes.barycentrics;
    payload.color = float4(saturate(float3(bary, 1.0 - bary.x - bary.y)), 1.0);
}
