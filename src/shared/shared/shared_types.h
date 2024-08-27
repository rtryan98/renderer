#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

#if __cplusplus
    #include <DirectXMath.h>
    // Structs
    #ifndef SHADER_STRUCT_ALIGN
        #define SHADER_STRUCT_ALIGN alignas(16) // HLSL structs are aligned to 16 bytes
    #endif
    // Utilities
    #ifndef SHADER_ENUM_CLASS
        #define SHADER_ENUM_CLASS class
    #endif
    // Bindless
    #ifndef SHADER_HANDLE_TYPE
        #define SHADER_HANDLE_TYPE uint32_t
    #endif
    // Scalars
    #ifndef uint
        #define uint uint32_t
    #endif
    // Vectors
    #ifndef float2
        #define float2 DirectX::XMFLOAT2
    #endif
    #ifndef float3
        #define float3 alignas(DirectX::XMFLOAT4) DirectX::XMFLOAT3
    #endif
    #ifndef float4
        #define float4 DirectX::XMFLOAT4
    #endif
    #ifndef uint2
        #define uint2 DirectX::XMUINT2
    #endif
    #ifndef uint3
        #define uint3 alignas(DirectX::XMUINT4) DirectX::XMUINT3
    #endif
    #ifndef uint4
        #define uint4 DirectX::XMUINT4
    #endif
    // Matrices
    #ifndef float3x3
        #define float3x3 DirectX::XMFLOAT3X3
    #endif
    #ifndef float4x4
        #define float4x4 DirectX::XMFLOAT4X4
    #endif
#else // HLSL
    // Structs
    #ifndef SHADER_STRUCT_ALIGN
        #define SHADER_STRUCT_ALIGN
    #endif
    // Utilities
    #ifndef SHADER_ENUM_CLASS
        #define SHADER_ENUM_CLASS
    #endif
    // Bindless
    #ifndef SHADER_HANDLE_TYPE
        #define SHADER_HANDLE_TYPE uint
    #endif
#endif

#endif
