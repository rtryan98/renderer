#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

#if __cplusplus
    #include <glm/glm.hpp>
    // Structs
    #ifndef SHADER_STRUCT_ALIGN
        #define SHADER_STRUCT_ALIGN alignas(16) // HLSL structs are aligned to 16 bytes in constant buffers
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
        #define float2 glm::vec2
    #endif
    #ifndef float3
        #define float3 glm::vec3
    #endif
    #ifndef float4
        #define float4 glm::vec4
    #endif
    #ifndef uint2
        #define uint2 glm::uvec2
    #endif
    #ifndef uint3
        #define uint3 glm::uvec3
    #endif
    #ifndef uint4
        #define uint4 glm::uvec4
    #endif
    // Matrices
    #ifndef float3x3
        #define float3x3 glm::mat3
    #endif
    #ifndef float3x4
        #define float3x4 glm::mat3x4
    #endif
    #ifndef float4x3
        #define float4x3 glm::mat4x3
    #endif
    #ifndef float4x4
        #define float4x4 glm::mat4
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
