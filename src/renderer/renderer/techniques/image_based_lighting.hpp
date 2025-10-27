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
    constexpr static auto ENVIRONMENT_CUBEMAP_TEXTURE_NAME = "image_based_lighting:environment_cubemap_texture";
    constexpr static auto PREFILTERED_DIFFUSE_IRRADIANCE_CUBEMAP_TEXTURE_NAME = "image_based_lighting:prefiltered_diffuse_irradiance_cubemap_texture";
    constexpr static auto PREFILTERED_SPECULAR_IRRADIANCE_CUBEMAP_TEXTURE_NAME = "image_based_lighting:prefiltered_specular_irradiance_cubemap_texture";
    constexpr static auto BRDF_LUT_TEXTURE_NAME = "image_based_lighting:brdf_lut_texture";

    Image_Based_Lighting(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Image_Based_Lighting();

    Image_Based_Lighting(const Image_Based_Lighting&) = delete;
    Image_Based_Lighting& operator=(const Image_Based_Lighting&) = delete;
    Image_Based_Lighting(Image_Based_Lighting&&) = delete;
    Image_Based_Lighting& operator=(Image_Based_Lighting&&) = delete;

    void bake(
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
    Image m_environment_cubemap = {};
    Image m_prefiltered_diffuse_irradiance_cubemap = {};
    Image m_prefiltered_specular_irradiance_cubemap = {};
    std::vector<Image_View> m_environment_cubemap_views = {};
    std::array<Image_View, 5> m_prefiltered_specular_irradiance_cubemap_views = {};

    bool m_baked = false;

private:
    void equirectangular_to_cubemap(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

    void prefilter_diffuse_irradiance(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

    void prefilter_specular_irradiance(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);
};
}
}
