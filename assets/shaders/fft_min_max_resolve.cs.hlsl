#include "rhi/bindless.hlsli"
#include "shared/fft_shared_types.h"

DECLARE_PUSH_CONSTANTS(FFT_Min_Max_Resolve_Push_Constants, pc);

// https://www.jeremyong.com/graphics/2023/09/05/f32-interlocked-min-max-hlsl/
uint order_preserving_float_mask(float value)
{
    uint uvalue = asuint(value);
    uint mask = -int(uvalue >> 31) | 0x80000000;
    return uvalue ^ mask;
}

template<typename T, uint COUNT>
T vec_order_preserving_float_map(T value)
{
    T result;
    for (uint i = 0; i < COUNT; ++i)
    {
        result[i] = order_preserving_float_mask(value[i]);
    }
    return result;
}

float inverse_order_preserving_float_map(uint value)
{
    uint mask = ((value >> 31) - 1) | 0x80000000;
    return asfloat(value ^ mask);
}

template<typename T, uint COUNT>
T vec_inverse_order_preserving_float_map(T value)
{
    T result;
    for (uint i = 0; i < COUNT; ++i)
    {
        result[i] = inverse_order_preserving_float_map(value[i]);
    }
    return result;
}

groupshared uint4 min_result;
groupshared uint4 max_result;

[shader("compute")]
[numthreads(FFT_MIN_MAX_RESOLVE_SIZE, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 min_value = rhi::uni::tex_load_arr<float4>(pc.min_max_tex, uint2(id.x, 0), id.z + pc.min_max_tex_load_offset);
    float4 max_value = rhi::uni::tex_load_arr<float4>(pc.min_max_tex, uint2(id.x, 1), id.z + pc.min_max_tex_load_offset);

    uint4 min_uvalue = vec_order_preserving_float_map<float4, 4>(min_value);
    uint4 max_uvalue = vec_order_preserving_float_map<float4, 4>(max_value);

    if (id.x == 0)
    {
        min_result = 0xffffffffu;
        max_result = 0x00000000u;
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint i = 0; i < 4; ++i)
    {
        InterlockedMin(min_result[i], min_uvalue[i]);
        InterlockedMax(max_result[i], max_uvalue[i]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (id.x == 0)
    {
        min_value = vec_inverse_order_preserving_float_map<float4, 4>(min_result);
        max_value = vec_inverse_order_preserving_float_map<float4, 4>(max_result);
        rhi::buf_store_arr(pc.min_max_buffer, 2 * id.z + 0 + pc.min_max_buffer_store_offset, min_value);
        rhi::buf_store_arr(pc.min_max_buffer, 2 * id.z + 1 + pc.min_max_buffer_store_offset, max_value);
    }
}
