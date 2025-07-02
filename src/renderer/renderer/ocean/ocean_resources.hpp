#pragma once

#include <rhi/resource.hpp>

#include <shared/ocean_shared_types.h>

namespace ren
{
class Application;
class Asset_Manager;
class Asset_Repository;

struct Ocean_Resources
{
    struct Options
    {
        bool use_fp16_textures = true;
        bool use_fp16_maths = false;
        uint32_t size = 256;
        uint32_t cascade_count = 4;

        auto operator<=>(const Options& other) const = default;

        rhi::Image_Create_Info generate_create_info(rhi::Image_Format format) const noexcept;
    } options;
    struct Data
    {
        Ocean_Initial_Spectrum_Data initial_spectrum_data;
        float total_time;
        bool update_time = true;
        bool debug_render_slope;
        bool debug_render_normal;
    } data;
    struct GPU_Resources
    {
        rhi::Sampler* linear_sampler;

        rhi::Image* initial_spectrum_texture;
        rhi::Image* angular_frequency_texture;
        rhi::Image* x_y_z_xdx_texture;
        rhi::Image* ydx_zdx_ydy_zdy_texture;

        rhi::Buffer* initial_spectrum_data;
        rhi::Buffer* vertex_buffer;
        rhi::Buffer* index_buffer;

        rhi::Pipeline* debug_render_slopes_pipeline;
        rhi::Pipeline* debug_render_normals_pipeline;
        rhi::Pipeline* render_patch_pipeline;

        rhi::Pipeline* render_composite_pipeline;
    } gpu_resources;

    void create_buffers(Asset_Manager& asset_manager);
    void destroy_buffers(Asset_Manager& asset_manager);
    void create_textures(Asset_Manager& asset_manager);
    void destroy_textures(Asset_Manager& asset_manager);
    void create_graphics_pipelines(Asset_Manager& asset_manager, Asset_Repository& asset_repository);
    void destroy_graphics_pipelines(Asset_Manager& asset_manager);
    void create_samplers(Asset_Manager& asset_manager);
    void destroy_samplers(Asset_Manager& asset_manager);
};
}
