#include "draw/basic_draw.hlsli"
#include "shared/draw_shared_types.h"
#include "rhi/bindless.hlsli"

DECLARE_PUSH_CONSTANTS(Immediate_Draw_Push_Constants, pc);

PS_Out main(PS_In ps_in)
{
    GPU_Material material = rhi::buf_load_arr<GPU_Material>(pc.material_instance_buffer, ps_in.material_index);

    float4 color = rhi::tex_sample<float4>(material.albedo, material.sampler_id, ps_in.tex_coord);
    if (color.a < 0.5) discard;

    float3 vn = normalize(ps_in.normal);
    float3 vt = normalize(ps_in.tangent.xyz);
    float3 vb = normalize(ps_in.tangent.w * cross(vn, vt));
    float3x3 TBN = float3x3(vt, vb, vn);

    float3 normal = 0.0;
    if (abs(ps_in.tangent.w) < 0.001)
    {
        normal = vn;
    }
    else
    {
        normal = mul(TBN, rhi::tex_sample<float4>(material.normal, material.sampler_id, ps_in.tex_coord).xyz * 2.0 - 1.0);
    }

    float2 metallic_roughness = rhi::tex_sample<float2>(material.metallic_roughness, material.sampler_id, ps_in.tex_coord).yx;

    PS_Out result = {
        color,
        float4(normal, 0.0),
        metallic_roughness
    };
    return result;
}
