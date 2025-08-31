#pragma once

#include "renderer/render_resource_blackboard.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;

namespace techniques
{
class Image_Based_Lighting
{
public:
    constexpr static auto HDRI_TEXTURE_NAME = "image_based_lighting:hdri_texture";
    constexpr static auto CUBEMAP_TEXTURE_NAME = "image_based_lighting:cubemap_texture";

    Image_Based_Lighting(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Image_Based_Lighting();

    Image_Based_Lighting(const Image_Based_Lighting&) = delete;
    Image_Based_Lighting& operator=(const Image_Based_Lighting&) = delete;
    Image_Based_Lighting(Image_Based_Lighting&&) = delete;
    Image_Based_Lighting& operator=(Image_Based_Lighting&&) = delete;

    void equirectangular_to_cubemap(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

    void skybox_render(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_geometry_render_target,
        const Image& geometry_depth_buffer);

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Image m_hdri = {};
    Image m_cubemap = {};
};
}
}
