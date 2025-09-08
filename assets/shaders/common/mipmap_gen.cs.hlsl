#include "rhi/bindless.hlsli"
#include "shared/mipmap_gen_shared_types.h"

DECLARE_PUSH_CONSTANTS(Mipmap_Gen_Push_Constants, pc);

[shader("compute")]
[numthreads(16,16,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint2 xy = uint2(id.xy * 2);

    static const uint2 OFFSETS[] = {
        uint2(0, 0),
        uint2(0, 1),
        uint2(1, 0),
        uint2(1, 1),
    };

    if (pc.is_array > 0)
    {
        float4 value = 0.0;
        for (uint i = 0; i < 4; ++i)
        {
            value += rhi::uni::tex_load_arr<float4>(pc.src, xy + OFFSETS[i], id.z);
        }
        rhi::uni::tex_store_arr(pc.dst, id.xy, id.z, value / 4.0);
    }
    else
    {
        float4 value = 0.0;
        for (uint i = 0; i < 4; ++i)
        {
            value += rhi::uni::tex_load<float4>(pc.src, xy + OFFSETS[i]);
        }
        rhi::uni::tex_store(pc.dst, id.xy, value / 4.0);
    }
}
