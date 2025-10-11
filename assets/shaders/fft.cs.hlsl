#include "rhi/bindless.hlsli"
#include "shared/fft_shared_types.h"
#include "shaders/complex.hlsli"
#include "shaders/constants.hlsli"

DECLARE_PUSH_CONSTANTS(FFT_Push_Constants, pc);

#if FFT_FLOAT4
#define FFT_VECTOR_LOAD_TYPE float4
#define FFT_VECTOR_TYPE float16_t4
#define FFT_VECTOR_UINT_TYPE uint4
#else
#define FFT_VECTOR_LOAD_TYPE float2
#define FFT_VECTOR_TYPE float16_t2
#define FFT_VECTOR_UINT_TYPE uint2
#endif

#define FFT_FLOAT_TYPE float16_t
#define FFT_VECTOR_2_TYPE float16_t2

groupshared FFT_VECTOR_TYPE ping_pong_buffer[2][FFT_SIZE];

#if FFT_STORE_MINMAX
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

groupshared FFT_VECTOR_UINT_TYPE min_result;
groupshared FFT_VECTOR_UINT_TYPE max_result;
#endif

void butterfly(uint tid, uint iter, out uint2 twiddle_indices, out FFT_VECTOR_2_TYPE twiddle_factor)
{
    uint butterfly_size = 2u << iter;
    uint butterfly_half_size = (butterfly_size >> 1u);
    uint butterfly_size_relative_idx = tid % butterfly_size;
    uint butterfly_start_idx = butterfly_size * (tid / butterfly_size);

    uint base_idx = butterfly_start_idx + (butterfly_size_relative_idx % butterfly_half_size);
    uint lower_idx = base_idx;
    uint upper_idx = base_idx + butterfly_half_size;
    twiddle_indices = uint2(lower_idx, upper_idx);

    FFT_FLOAT_TYPE arg = FFT_FLOAT_TYPE(-ren::TWO_PI) * FFT_FLOAT_TYPE(butterfly_size_relative_idx) / FFT_FLOAT_TYPE(butterfly_size);
    sincos(arg, twiddle_factor.y, twiddle_factor.x);
    if (pc.inverse) twiddle_factor = ren::cconjugate(twiddle_factor);
}

[numthreads(FFT_SIZE, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    bool ping_pong = false;
    uint2 texpos = bool(pc.vertical_or_horizontal) ? id.xy : id.yx;
    // TODO: <HACK> SPIR-V compile crashes here for some reason without the weird cast. Change this workaround to not waste bandwidth in fp16 mode.
    ping_pong_buffer[ping_pong][reversebits(id.x) >> (32 - FFT_LOG_SIZE)] = FFT_VECTOR_TYPE(rhi::uni::tex_load_arr<FFT_VECTOR_LOAD_TYPE>(pc.image, texpos, id.z));
    GroupMemoryBarrierWithGroupSync();

    [unroll(FFT_LOG_SIZE)] for (uint i = 0; i < FFT_LOG_SIZE; ++i)
    {
        uint2 twiddle_indices;
        FFT_VECTOR_2_TYPE twiddle_factor;
        butterfly(id.x, i, twiddle_indices, twiddle_factor);
        FFT_VECTOR_TYPE result;
        FFT_VECTOR_TYPE twiddle_x = ping_pong_buffer[ping_pong][twiddle_indices.x];
        FFT_VECTOR_TYPE twiddle_y = ping_pong_buffer[ping_pong][twiddle_indices.y];
        result.xy = twiddle_x.xy + ren::cmul(twiddle_y.xy, twiddle_factor);
        #if FFT_FLOAT4
        result.zw = twiddle_x.zw + ren::cmul(twiddle_y.zw, twiddle_factor);
        #endif
        ping_pong_buffer[!ping_pong][id.x] = result;
        GroupMemoryBarrierWithGroupSync();
        ping_pong = !ping_pong;
    }

    FFT_VECTOR_TYPE result = ping_pong_buffer[ping_pong][id.x];
    FFT_VECTOR_LOAD_TYPE result_fp32 = (FFT_VECTOR_LOAD_TYPE) result;

    rhi::uni::tex_store_arr(pc.image, texpos, id.z, result_fp32);

#if FFT_STORE_MINMAX
#if FFT_FLOAT4
#define ELEMENT_COUNT 4
#else
#define ELEMENT_COUNT 2
#endif
    vector<uint, ELEMENT_COUNT> values = vec_order_preserving_float_map<FFT_VECTOR_LOAD_TYPE, ELEMENT_COUNT>(result_fp32);
    if (id.x == 0)
    {
        min_result = 0xffffffffu;
        max_result = 0x00000000u;
    }
    GroupMemoryBarrierWithGroupSync();
    for (uint i = 0; i < ELEMENT_COUNT; ++i)
    {
        InterlockedMin(min_result[i], values[i]);
        InterlockedMax(max_result[i], values[i]);
    }
    GroupMemoryBarrierWithGroupSync();
    if (id.x == 0)
    {
        FFT_VECTOR_LOAD_TYPE min_results = vec_inverse_order_preserving_float_map<FFT_VECTOR_LOAD_TYPE, ELEMENT_COUNT>(min_result);
        FFT_VECTOR_LOAD_TYPE max_results = vec_inverse_order_preserving_float_map<FFT_VECTOR_LOAD_TYPE, ELEMENT_COUNT>(max_result);
        rhi::tex_store_arr(pc.min_max_tex, uint2(id.y, 0), id.z + pc.min_max_tex_store_offset, min_results);
        rhi::tex_store_arr(pc.min_max_tex, uint2(id.y, 1), id.z + pc.min_max_tex_store_offset, max_results);
    }
#endif
}
