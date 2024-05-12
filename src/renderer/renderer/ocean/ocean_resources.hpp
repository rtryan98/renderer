#pragma once

#include <rhi/resource.hpp>

#include "shaders/ocean/ocean_shared_types.hlsli"

namespace ren
{
struct Ocean_Resources
{
    struct Options
    {
        bool use_fp16_textures = true;
        bool use_fp16_maths = false;
        uint32_t size = 256;
        uint32_t cascade_count = 1;

        auto operator<=>(const Options& other) const = default;

        rhi::Image_Create_Info generate_create_info() const noexcept
        {
            return {
                .format = use_fp16_textures
                    ? rhi::Image_Format::R16G16B16A16_SFLOAT
                    : rhi::Image_Format::R32G32B32A32_SFLOAT,
                .width = size,
                .height = size,
                .depth = 1,
                .array_size = uint16_t(cascade_count),
                .mip_levels = 1,
                .usage = rhi::Image_Usage::Unordered_Access | rhi::Image_Usage::Sampled,
                .primary_view_type = rhi::Image_View_Type::Texture_2D_Array
            };
        }
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
};
}
