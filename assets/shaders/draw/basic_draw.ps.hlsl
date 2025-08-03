#include "draw/basic_draw.hlsli"
#include "shared/draw_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Immediate_Draw_Push_Constants, pc);

PS_Out main(PS_In ps_in)
{
    GPU_Material material = rhi::buf_load_arr<GPU_Material>(pc.material_instance_buffer, ps_in.material_index);

    float4 color = rhi::tex_sample<float4>(material.albedo, material.sampler_id, ps_in.tex_coord);
    if (color.a < 0.001) discard;
    
    float4 normal = rhi::tex_sample<float4>(material.normal, material.sampler_id, ps_in.tex_coord);
    float2 metallic_roughness = rhi::tex_sample<float2>(material.metallic_roughness, material.sampler_id, ps_in.tex_coord);

    PS_Out result = {
        color,
        normal,
        metallic_roughness
    };
    return result;
}
