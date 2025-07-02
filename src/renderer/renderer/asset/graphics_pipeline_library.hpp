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
}
