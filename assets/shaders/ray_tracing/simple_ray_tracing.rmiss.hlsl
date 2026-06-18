// SHADER DEF simple_ray_tracing
// ENTRYPOINT main
// TYPE rmiss
// SHADER END DEF

struct [raypayload] Simple_Payload
{
    float4 color : write(closesthit, miss, caller) : read(caller);
};

[shader("miss")]
void main(inout Simple_Payload payload)
{
    payload.color = float4(0.5, 0.0, 0.0, 1.0);
}
