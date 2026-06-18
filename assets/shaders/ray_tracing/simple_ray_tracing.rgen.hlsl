// SHADER DEF simple_ray_tracing
// ENTRYPOINT main
// TYPE rgen
// SHADER END DEF

#include "rhi/bindless.hlsli"
#include "shared/camera_shared_types.h"

struct Push_Constants
{
    SHADER_HANDLE_TYPE tlas;
    SHADER_HANDLE_TYPE camera;
    SHADER_HANDLE_TYPE output;
};

DECLARE_PUSH_CONSTANTS(Push_Constants, pc);

struct [raypayload] Simple_Payload
{
    float4 color : write(closesthit, miss, caller) : read(caller);
};

void generate_camera_ray(uint2 index, out float3 origin, out float3 direction)
{
    GPU_Camera_Data camera = rhi::buf_load<GPU_Camera_Data>(pc.camera);

    float2 xy = index + 0.5;
    float2 screen = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;
    screen.y = -screen.y;
    float4 world = mul(camera.clip_to_world, float4(screen, 0., 1.));
    world.xyz /= world.w;

    origin = camera.position.xyz;
    direction = normalize(world.xyz - origin);
}

[shader("raygeneration")]
void main()
{
    float3 ray_dir = 0.;
    float3 ray_origin = 0.;
    generate_camera_ray(DispatchRaysIndex().xy, ray_origin, ray_dir);

    RayDesc ray = {
        ray_origin,
        0.1,
        ray_dir,
        500.0
    };
    Simple_Payload payload = { float4(0.25, 0.5, 0.0, 0.0) };

    TraceRay(rhi::get_rtas(pc.tlas), RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0u, 0, 0, 0, ray, payload);

    rhi::tex_store(pc.output, DispatchRaysIndex().xy, payload.color);
}
