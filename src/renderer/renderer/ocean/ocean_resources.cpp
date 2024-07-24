#include "renderer/ocean/ocean_resources.hpp"

#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"

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

void Ocean_Resources::create_graphics_pipelines(Asset_Manager& asset_manager, Shader_Library& shader_library)
{
    destroy_graphics_pipelines(asset_manager);

    auto debug_ci = rhi::Graphics_Pipeline_Create_Info {
        .vs = shader_library.get_shader(Shaders::debug_render_ocean_normal_vs),
        .ps = shader_library.get_shader(Shaders::debug_render_ocean_ps),
        .blend_state_info = {
            .independent_blend_enable = false,
            .color_attachments = {
                rhi::Pipeline_Color_Attachment_Blend_Info {
                .blend_enable = false,
                .logic_op_enable = false
                }
            }
        },
        .rasterizer_state_info = {
            .fill_mode = rhi::Fill_Mode::Solid,
            .cull_mode = rhi::Cull_Mode::None,
            .winding_order = rhi::Winding_Order::Front_Face_CCW
        },
        .depth_stencil_info = {
            .depth_enable = true,
            .depth_write_enable = true,
            .comparison_func = rhi::Comparison_Func::Less_Equal
        },
        .primitive_topology = rhi::Primitive_Topology_Type::Line,
        .color_attachment_count = 1,
        .color_attachment_formats = { rhi::Image_Format::R8G8B8A8_SRGB },
        .depth_stencil_format = rhi::Image_Format::D32_SFLOAT
    };

    gpu_resources.debug_render_normals_pipeline = asset_manager.create_pipeline(debug_ci);
    debug_ci.vs = shader_library.get_shader(Shaders::debug_render_ocean_slope_vs);
    gpu_resources.debug_render_slopes_pipeline = asset_manager.create_pipeline(debug_ci);
}

void Ocean_Resources::destroy_graphics_pipelines(Asset_Manager& asset_manager)
{
    if (gpu_resources.debug_render_normals_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.debug_render_normals_pipeline);
    if (gpu_resources.debug_render_slopes_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.debug_render_slopes_pipeline);
}

void Ocean_Resources::create_compute_pipelines(Asset_Manager& asset_manager, Shader_Library& shader_library)
{
    destroy_compute_pipelines(asset_manager);

    gpu_resources.initial_spectrum_pipeline = asset_manager.create_pipeline({
        .cs = shader_library.get_shader(Shaders::initial_spectrum_cs)});
    gpu_resources.time_dependent_spectrum_pipeline = asset_manager.create_pipeline({
        .cs = shader_library.get_shader(Shaders::time_dependent_spectrum_cs)});
    gpu_resources.fft_pipeline = asset_manager.create_pipeline({
        .cs = shader_library.get_shader(
            select_fft_shader(
                options.size,
                options.use_fp16_maths,
                true))});
}

void Ocean_Resources::destroy_compute_pipelines(Asset_Manager& asset_manager)
{
    if (gpu_resources.initial_spectrum_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.initial_spectrum_pipeline);
    if (gpu_resources.time_dependent_spectrum_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.time_dependent_spectrum_pipeline);
    if (gpu_resources.fft_pipeline)
        asset_manager.destroy_pipeline(gpu_resources.fft_pipeline);
}

}
