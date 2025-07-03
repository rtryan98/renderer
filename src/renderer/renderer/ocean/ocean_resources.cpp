#include "renderer/ocean/ocean_resources.hpp"

#include "renderer/application.hpp"
#include "renderer/asset_manager.hpp"
#include "renderer/asset/asset_repository.hpp"

namespace ren
{
rhi::Image_Create_Info Ocean_Resources::Options::generate_create_info(rhi::Image_Format format) const noexcept
{
    return {
        .format = format, //use_fp16_textures
        //? rhi::Image_Format::R16G16B16A16_SFLOAT
        //: rhi::Image_Format::R32G32B32A32_SFLOAT,
        .width = size,
        .height = size,
        .depth = 1,
        .array_size = uint16_t(cascade_count),
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Unordered_Access | rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D_Array
    };
}

void Ocean_Resources::create_buffers(Asset_Manager& asset_manager)
{
    rhi::Buffer_Create_Info initial_spectrum_data_buffer_create_info =  {
        .size = sizeof(data.initial_spectrum_data),
        .heap = rhi::Memory_Heap_Type::GPU
    };
    gpu_resources.initial_spectrum_data = asset_manager.create_buffer(initial_spectrum_data_buffer_create_info);
}

void Ocean_Resources::destroy_buffers(Asset_Manager& asset_manager)
{
    if (gpu_resources.initial_spectrum_data)
        asset_manager.destroy_buffer(gpu_resources.initial_spectrum_data);
}

void Ocean_Resources::create_textures(Asset_Manager& asset_manager)
{
    destroy_textures(asset_manager);

    auto format_rgba = options.use_fp16_textures
        ? rhi::Image_Format::R16G16B16A16_SFLOAT
        : rhi::Image_Format::R32G32B32A32_SFLOAT;
    auto image_create_info_rgba = options.generate_create_info(format_rgba);
    auto format_r = options.use_fp16_textures
        ? rhi::Image_Format::R16_SFLOAT
        : rhi::Image_Format::R32_SFLOAT;
    auto image_create_info_r = options.generate_create_info(format_r);

    gpu_resources.initial_spectrum_texture = asset_manager.create_image(image_create_info_rgba, "Ocean initial spectrum");
    gpu_resources.angular_frequency_texture = asset_manager.create_image(image_create_info_r, "Ocean angular frequency");
    gpu_resources.x_y_z_xdx_texture = asset_manager.create_image(image_create_info_rgba, "Ocean x,y,z,xdx");
    gpu_resources.ydx_zdx_ydy_zdy_texture = asset_manager.create_image(image_create_info_rgba, "Ocean ydx,zdx,ydy,zdy");
}

void Ocean_Resources::destroy_textures(Asset_Manager& asset_manager)
{
    if (gpu_resources.initial_spectrum_texture)
        asset_manager.destroy_image(gpu_resources.initial_spectrum_texture);
    if (gpu_resources.angular_frequency_texture)
        asset_manager.destroy_image(gpu_resources.angular_frequency_texture);
    if (gpu_resources.x_y_z_xdx_texture)
        asset_manager.destroy_image(gpu_resources.x_y_z_xdx_texture);
    if (gpu_resources.ydx_zdx_ydy_zdy_texture)
        asset_manager.destroy_image(gpu_resources.ydx_zdx_ydy_zdy_texture);
}

void Ocean_Resources::create_samplers(Asset_Manager& asset_manager)
{
    rhi::Sampler_Create_Info create_info = {
        .filter_min = rhi::Sampler_Filter::Linear,
        .filter_mag = rhi::Sampler_Filter::Linear,
        .filter_mip = rhi::Sampler_Filter::Linear,
        .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
        .mip_lod_bias = 0.f,
        .max_anisotropy = 16,
        .comparison_func = rhi::Comparison_Func::None,
        .reduction = rhi::Sampler_Reduction_Type::Standard,
        .border_color = {},
        .min_lod = .0f,
        .max_lod = .0f,
        .anisotropy_enable = true
    };
    gpu_resources.linear_sampler = asset_manager.create_sampler(create_info);
}

void Ocean_Resources::destroy_samplers(Asset_Manager& asset_manager)
{
    if (gpu_resources.linear_sampler)
        asset_manager.destroy_sampler(gpu_resources.linear_sampler);
}

}
