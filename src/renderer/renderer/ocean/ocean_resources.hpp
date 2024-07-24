#pragma once

#include <rhi/resource.hpp>

#include <shared/ocean_shared_types.h>

namespace ren
{
class Asset_Manager;
class Shader_Library;

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
    } data;
    struct GPU_Resources
    {
        rhi::Image* initial_spectrum_texture;
        rhi::Image* angular_frequency_texture;
        rhi::Image* x_y_z_xdx_texture;
        rhi::Image* ydx_zdx_ydy_zdy_texture;

        rhi::Buffer* initial_spectrum_data;
        rhi::Buffer* vertex_buffer;
        rhi::Buffer* index_buffer;

        rhi::Pipeline* initial_spectrum_pipeline;
        rhi::Pipeline* time_dependent_spectrum_pipeline;
        rhi::Pipeline* fft_pipeline;

        rhi::Pipeline* debug_render_slopes_pipeline;
        rhi::Pipeline* debug_render_normals_pipeline;
    } gpu_resources;

    void create_buffers(Asset_Manager& asset_manager);
    void destroy_buffers(Asset_Manager& asset_manager);
    void create_textures(Asset_Manager& asset_manager);
    void destroy_textures(Asset_Manager& asset_manager);
    void create_graphics_pipelines(Asset_Manager& asset_manager, Shader_Library& shader_library);
    void destroy_graphics_pipelines(Asset_Manager& asset_manager);
    void create_compute_pipelines(Asset_Manager& asset_manager, Shader_Library& shader_library);
    void destroy_compute_pipelines(Asset_Manager& asset_manager);
};
}
