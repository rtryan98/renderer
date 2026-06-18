#pragma once
#include <rhi/resource.hpp>
#include <string>

namespace ren
{
enum class Graphics_Pipeline_Type
{
    Vertex,
    Mesh
};

struct Shader_Library;

struct Graphics_Pipeline_Library
{
    Shader_Library* ts;
    std::string ts_variant;
    Shader_Library* ms;
    std::string ms_variant;
    Shader_Library* vs;
    std::string vs_variant;
    Shader_Library* ps;
    std::string ps_variant;
    rhi::Pipeline_Blend_State_Info blend_state_info{};
    rhi::Primitive_Topology_Type primitive_topology;
    rhi::Pipeline_Rasterization_State_Info rasterizer_state_info;
    rhi::Pipeline_Depth_Stencil_State_Info depth_stencil_info;
    std::array<rhi::Image_Format, rhi::PIPELINE_COLOR_ATTACHMENTS_MAX> color_attachments{};
    uint32_t color_attachment_count;
    rhi::Image_Format depth_stencil_format;

    rhi::Pipeline* pipeline;
};

struct Ray_Tracing_Shader_Ref
{
    Shader_Library* lib;
    std::string variant;
};

struct Ray_Tracing_Pipeline_Library
{
    // TODO: variant support
    std::vector<Ray_Tracing_Shader_Ref> shaders;
    std::vector<rhi::Ray_Tracing_Hit_Group> hit_groups;
    std::vector<uint32_t> ray_gen_libraries;
    std::vector<uint32_t> miss_libraries;
    std::vector<uint32_t> callable_libraries;
    uint32_t max_recursion_depth;
    uint32_t max_payload_size;
    uint32_t max_attribute_size;

    rhi::Pipeline* pipeline;
};
}
