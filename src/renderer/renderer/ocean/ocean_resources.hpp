#pragma once

#include <rhi/resource.hpp>

#include "shaders/ocean/ocean_shared_types.hlsli"

namespace ren
{
class Asset_Manager;

struct Ocean_Resources
{
    struct Options
    {
        bool use_fp16_textures = true;
        bool use_fp16_maths = false;
        uint32_t size = 256;
        uint32_t cascade_count = 1;

        auto operator<=>(const Options& other) const = default;

        rhi::Image_Create_Info generate_create_info() const noexcept;
    } options;
    struct Data
    {
        Ocean_Initial_Spectrum_Data initial_spectrum_data;
        Ocean_Time_Dependent_Spectrum_Data time_dependent_spectrum_data;
    } data;
    struct GPU_Resources
    {
        rhi::Image* spectrum_texture;

        rhi::Pipeline* fft_pipeline;
    } gpu_resources;

    void create_textures(Asset_Manager& asset_manager);
};
}
