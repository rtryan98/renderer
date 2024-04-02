#ifndef REN_UTIL
#define REN_UTIL

namespace ren
{
    float4 unpack_unorm_4x8(uint value)
    {
        return float4(
            float((value >> 24) & 0xFF) / 255.0,
            float((value >> 16) & 0xFF) / 255.0,
            float((value >>  8) & 0xFF) / 255.0,
            float((value >>  0) & 0xFF) / 255.0
        );
    }
}

#endif
