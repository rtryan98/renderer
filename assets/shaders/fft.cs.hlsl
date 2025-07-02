#include "rhi/bindless.hlsli"
#include "shared/fft_shared_types.h"
#include "shaders/complex.hlsli"
#include "shaders/constants.hlsli"

DECLARE_PUSH_CONSTANTS(FFT_Push_Constants, pc);

#if FFT_FLOAT4
#define FFT_VECTOR_TYPE float4
#else
#define FFT_VECTOR_TYPE float2
#endif

groupshared FFT_VECTOR_TYPE ping_pong_buffer[2][FFT_SIZE];

void butterfly(uint tid, uint iter, out uint2 twiddle_indices, out float2 twiddle_factor)
{
    uint butterfly_size = 2u << iter;
    uint butterfly_half_size = (butterfly_size >> 1u);
    uint butterfly_size_relative_idx = tid % butterfly_size;
    uint butterfly_start_idx = butterfly_size * (tid / butterfly_size);

    uint base_idx = butterfly_start_idx + (butterfly_size_relative_idx % butterfly_half_size);
    uint lower_idx = base_idx;
    uint upper_idx = base_idx + butterfly_half_size;
    twiddle_indices = uint2(lower_idx, upper_idx);

    float arg = -ren::TWO_PI * float(butterfly_size_relative_idx) / float(butterfly_size);
    sincos(arg, twiddle_factor.y, twiddle_factor.x);
    if (pc.inverse) twiddle_factor = ren::cconjugate(twiddle_factor);
}

[numthreads(FFT_SIZE, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    bool ping_pong = false;
    uint2 texpos = bool(pc.vertical_or_horizontal) ? id.xy : id.yx;
    rhi::RW_Texture image = { pc.image };
    ping_pong_buffer[ping_pong][reversebits(id.x) >> (32 - FFT_LOG_SIZE)] = image.load_2d_array_uniform<FFT_VECTOR_TYPE>(uint3(texpos, id.z));
    GroupMemoryBarrierWithGroupSync();

    [unroll(FFT_LOG_SIZE)] for (uint i = 0; i < FFT_LOG_SIZE; ++i)
    {
        uint2 twiddle_indices;
        float2 twiddle_factor;
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

    image.store_2d_array_uniform(uint3(texpos, id.z), ping_pong_buffer[ping_pong][id.x]);
}
