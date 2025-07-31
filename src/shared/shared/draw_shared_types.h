#ifndef DRAW_SHARED_TYPES
#define DRAW_SHARED_TYPES
#include "shared/shared_types.h"

struct SHADER_STRUCT_ALIGN Immediate_Draw_Push_Constants
{
    SHADER_HANDLE_TYPE position_buffer;
    SHADER_HANDLE_TYPE attribute_buffer;
    SHADER_HANDLE_TYPE camera_buffer;
    SHADER_HANDLE_TYPE instance_indices_buffer;
    SHADER_HANDLE_TYPE instance_transform_buffer;
    SHADER_HANDLE_TYPE material_instance_buffer;
};

struct GPU_Instance_Indices
{
    uint transform_index;
    uint material_index;
};

struct GPU_Instance_Transform_Data
{
    float4x4 mesh_to_world;
    float3x3 normal_to_world;
};

struct GPU_Material
{
    uint base_color_factor;
    float pbr_roughness;
    float pbr_metallic;
    float3 emissive_color;
    float emissive_strength;
    SHADER_HANDLE_TYPE albedo;
    SHADER_HANDLE_TYPE normal;
    SHADER_HANDLE_TYPE metallic_roughness;
    SHADER_HANDLE_TYPE emissive;
    SHADER_HANDLE_TYPE sampler_id;
};

#endif
