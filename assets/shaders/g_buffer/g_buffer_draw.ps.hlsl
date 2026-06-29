// SHADER DEF g_buffer_draw
// ENTRYPOINT main
// TYPE ps
// SHADER END DEF

#include "g_buffer/g_buffer_draw.hlsli"
#include "shared/draw_shared_types.h"
#include "shared/shared_resources.h"
#include "rhi/bindless.hlsli"
#include "util.hlsli"
#include "shaders/common/octahedron_encoding.hlsli"

DECLARE_PUSH_CONSTANTS(Immediate_Draw_Push_Constants, pc);

float2 compute_uv_mv(float4 position_current, float4 position_prev)
{
    float2 ndc_current = position_current.xy / position_current.w;
    float2 ndc_prev = position_prev.xy / position_prev.w;
    float2 uv_current = float2(0.5, -0.5) * ndc_current + 0.5;
    float2 uv_prev = float2(0.5, -0.5) * ndc_prev + 0.5;
    return uv_prev - uv_current;
}

PS_Out main(PS_In ps_in)
{
    GPU_Material material = rhi::buf_load_arr<GPU_Material>(REN_GLOBAL_MATERIAL_INSTANCE_BUFFER, ps_in.material_index);

    float4 color = rhi::tex_sample<float4>(material.albedo, material.sampler_id, ps_in.tex_coord);
    color *= ren::unpack_unorm_4x8(material.base_color_factor);
    if (color.a < 0.5) discard;

    float3 vn = normalize(ps_in.normal);
    float3 vt = normalize(ps_in.tangent.xyz);
    float3 vb = normalize(ps_in.tangent.w * cross(vn, vt));
    float3x3 TBN = transpose(float3x3(vt, vb, vn));

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
    metallic_roughness.x *= material.pbr_metallic;
    metallic_roughness.y *= material.pbr_roughness;
    normal = ren::oct_signed_encode(normal);

    PS_Out result = {
        float4(color.xyz, 1.0),
        float4(normal.xy, 0., normal.z),
        metallic_roughness,
        compute_uv_mv(ps_in.position_clip, ps_in.prev_position_clip)
    };
    return result;
}
