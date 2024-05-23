#include "renderer/ocean/ocean_resources.hpp"

#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"

namespace ren
{
rhi::Image_Create_Info Ocean_Resources::Options::generate_create_info() const noexcept
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

void Ocean_Resources::create_textures(Asset_Manager& asset_manager)
{
    destroy_textures(asset_manager);

    auto image_create_info = options.generate_create_info();
    gpu_resources.spectrum_texture = asset_manager.create_image(image_create_info);
}

void Ocean_Resources::destroy_textures(Asset_Manager& asset_manager)
{
    if (gpu_resources.spectrum_texture)
        asset_manager.destroy_image(gpu_resources.spectrum_texture);
}

void Ocean_Resources::create_pipelines(Asset_Manager& asset_manager, Shader_Library& shader_library)
{
    destroy_pipelines(asset_manager);

    gpu_resources.fft_pipeline = asset_manager.create_pipeline({
        .cs = shader_library.get_shader(
            select_fft_shader(
                options.size,
                options.use_fp16_maths,
                true))});
}

void Ocean_Resources::destroy_pipelines(Asset_Manager& asset_manager)
{
    if (gpu_resources.fft_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.fft_pipeline);
}

}
