#pragma once

#include <rhi/resource.hpp>
#include <renderer/render_resource_blackboard.hpp>

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
        Sampler linear_sampler;

        Image initial_spectrum_texture;
        Image angular_frequency_texture;
        Image x_y_z_xdx_texture;
        Image ydx_zdx_ydy_zdy_texture;

        Buffer initial_spectrum_data;
    } gpu_resources;

    void create_buffers(Render_Resource_Blackboard& resource_blackboard);
    void destroy_buffers(Render_Resource_Blackboard& resource_blackboard);
    void create_textures(Render_Resource_Blackboard& resource_blackboard);
    void destroy_textures(Render_Resource_Blackboard& resource_blackboard);
    void create_samplers(Render_Resource_Blackboard& resource_blackboard);
};
}
