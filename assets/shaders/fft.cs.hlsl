#include "rhi/bindless.hlsli"
#include "shared/fft_shared_types.h"
#include "shaders/complex.hlsli"
#include "shaders/constants.hlsli"

DECLARE_PUSH_CONSTANTS(FFT_Push_Constants, pc);

#if FFT_FLOAT4
#define FFT_VECTOR_LOAD_TYPE float4
#if FFT_FP16
#define FFT_VECTOR_TYPE float16_t4
#else
#define FFT_VECTOR_TYPE float4
#endif
#else
#define FFT_VECTOR_LOAD_TYPE float2
#if FFT_FP16
#define FFT_VECTOR_TYPE float16_t2
#else
#define FFT_VECTOR_TYPE float2
#endif
#endif

#if FFT_FP16
#define FFT_FLOAT_TYPE float16_t
#define FFT_VECTOR_2_TYPE float16_t2
#else
#define FFT_FLOAT_TYPE float
#define FFT_VECTOR_2_TYPE float2
#endif


groupshared FFT_VECTOR_TYPE ping_pong_buffer[2][FFT_SIZE];

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

    rhi::uni::tex_store_arr(pc.image, texpos, id.z, FFT_VECTOR_LOAD_TYPE(ping_pong_buffer[ping_pong][id.x]));
}
