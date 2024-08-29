#include "renderer/ocean/ocean_renderer.hpp"

#include "renderer/application.hpp"
#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"

#include <rhi/command_list.hpp>
#include <shared/fft_shared_types.h>
#include <shared/ocean_shared_types.h>

namespace ren
{
Ocean_Renderer::Ocean_Renderer(Asset_Manager& asset_manager, Shader_Library& shader_library)
    : m_asset_manager(asset_manager)
    , m_shader_library(shader_library)
    , m_resources()
    , m_settings(m_resources, m_asset_manager, m_shader_library)
{
    m_resources.create_buffers(m_asset_manager);
    m_resources.create_textures(m_asset_manager);
    m_resources.create_compute_pipelines(m_asset_manager, m_shader_library);
    m_resources.create_graphics_pipelines(m_asset_manager, m_shader_library);
    m_resources.create_samplers(m_asset_manager);

    float larges_lengthscale = 1024.f;
    auto calc_lengthscale = [](float lengthscale, uint32_t factor) {
        for (auto i = 0; i < factor; ++i)
        {
            lengthscale = lengthscale * (1.f - 1. / 1.681033988f);
        }
        return lengthscale;
    };
    m_resources.data.initial_spectrum_data = {
        .spectra = {
            {
                .u = 4.f,
                .f = 750.f,
                .phillips_alpha = 1.f,
                .generalized_a = 1.f,
                .generalized_b = 1.f,
                .contribution = 1.f,
                .wind_direction = 0.f
            },
            {
                .u = 7.5f,
                .f = 1000.f,
                .phillips_alpha = 1.f,
                .generalized_a = 1.f,
                .generalized_b = 1.f,
                .contribution = .75f,
                .wind_direction = 135.f
            }
        },
        .active_cascades = { true, true, true, true },
        .length_scales = {
            larges_lengthscale,
            calc_lengthscale(larges_lengthscale, 1),
            calc_lengthscale(larges_lengthscale, 4),
            calc_lengthscale(larges_lengthscale, 6)
        },
        .spectrum = uint32_t(Ocean_Spectrum::TMA),
        .directional_spreading_function = uint32_t(Ocean_Directional_Spreading_Function::Donelan_Banner),
        .texture_size = m_resources.options.size,
        .g = 9.81f,
        .h = 150.f,
    };
}

Ocean_Renderer::~Ocean_Renderer()
{
    m_resources.destroy_buffers(m_asset_manager);
    m_resources.destroy_textures(m_asset_manager);
    m_resources.destroy_compute_pipelines(m_asset_manager);
    m_resources.destroy_graphics_pipelines(m_asset_manager);
    m_resources.destroy_samplers(m_asset_manager);
}

Ocean_Settings* Ocean_Renderer::get_settings() noexcept
{
    return &m_settings;
}

void Ocean_Renderer::simulate(Application& app, rhi::Command_List* cmd, float dt) noexcept
{
    if (m_resources.data.update_time)
        m_resources.data.total_time += dt;

    app.upload_buffer_data_immediate(
        m_resources.gpu_resources.initial_spectrum_data,
        &m_resources.data.initial_spectrum_data, sizeof(Ocean_Initial_Spectrum_Data), 0);

    cmd->begin_debug_region("Ocean Simulation", 0.5f, 0.5f, 1.f);

    rhi::Image_Barrier_Info initial_spectrum_barrier_info = {
        .stage_before = rhi::Barrier_Pipeline_Stage::None,
        .stage_after = rhi::Barrier_Pipeline_Stage::Compute_Shader,
        .access_before = rhi::Barrier_Access::None,
        .access_after = rhi::Barrier_Access::Unordered_Access_Write,
        .layout_before = rhi::Barrier_Image_Layout::Undefined,
        .layout_after = rhi::Barrier_Image_Layout::Unordered_Access,
        .image = m_resources.gpu_resources.initial_spectrum_texture,
        .subresource_range = {
            .first_mip_level = 0,
            .mip_count = m_resources.gpu_resources.initial_spectrum_texture->mip_levels,
            .first_array_index = 0,
            .array_size = m_resources.gpu_resources.initial_spectrum_texture->array_size,
            .first_plane = 0,
            .plane_count = 1
        },
        .discard = true
    };
    auto angular_frequency_barrier_info = initial_spectrum_barrier_info;
    angular_frequency_barrier_info.image = m_resources.gpu_resources.angular_frequency_texture;

    {
        auto image_barriers = std::to_array({
            initial_spectrum_barrier_info,
            angular_frequency_barrier_info
            });
        rhi::Barrier_Info barrier = { .image_barriers = image_barriers };
        cmd->barrier(barrier);
    }

    cmd->begin_debug_region("Initial Spectrum", 0.25f, 0.0f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.initial_spectrum_pipeline);
    uint32_t tex_dispatch_size_xy =
        m_resources.options.size /
        m_resources.gpu_resources.initial_spectrum_pipeline->compute_shading_info.cs->groups_x;
    cmd->set_push_constants<Ocean_Initial_Spectrum_Push_Constants>({
        m_resources.gpu_resources.initial_spectrum_data->buffer_view->bindless_index,
        m_resources.gpu_resources.initial_spectrum_texture->image_view->bindless_index,
        m_resources.gpu_resources.angular_frequency_texture->image_view->bindless_index },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(tex_dispatch_size_xy, tex_dispatch_size_xy, m_resources.options.cascade_count);
    cmd->end_debug_region();

    auto x_y_z_xdx_barrier_info = initial_spectrum_barrier_info;
    x_y_z_xdx_barrier_info.image = m_resources.gpu_resources.x_y_z_xdx_texture;
    auto ydx_zdx_ydy_zdy_barrier_info = initial_spectrum_barrier_info;
    ydx_zdx_ydy_zdy_barrier_info.image = m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture;

    initial_spectrum_barrier_info.discard
        = angular_frequency_barrier_info.discard
        = false;
    initial_spectrum_barrier_info.stage_before
        = angular_frequency_barrier_info.stage_before
        = rhi::Barrier_Pipeline_Stage::Compute_Shader;
    initial_spectrum_barrier_info.access_before
        = angular_frequency_barrier_info.access_before
        = rhi::Barrier_Access::Unordered_Access_Write;
    initial_spectrum_barrier_info.access_after
        = angular_frequency_barrier_info.access_after
        = rhi::Barrier_Access::Shader_Read;
    initial_spectrum_barrier_info.layout_before
        = angular_frequency_barrier_info.layout_before
        = rhi::Barrier_Image_Layout::Unordered_Access;
    initial_spectrum_barrier_info.layout_after
        = angular_frequency_barrier_info.layout_after
        = rhi::Barrier_Image_Layout::Shader_Read_Only;

    {
        auto image_barriers = std::to_array({
            initial_spectrum_barrier_info,
            angular_frequency_barrier_info,
            x_y_z_xdx_barrier_info,
            ydx_zdx_ydy_zdy_barrier_info
            });
        rhi::Barrier_Info barrier = { .image_barriers = image_barriers };
        cmd->barrier(barrier);
    }

    x_y_z_xdx_barrier_info.discard
        = ydx_zdx_ydy_zdy_barrier_info.discard
        = false;

    cmd->begin_debug_region("Time Dependent Spectrum", 0.25f, 0.25f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.time_dependent_spectrum_pipeline);
    cmd->set_push_constants<Ocean_Time_Dependent_Spectrum_Push_Constants>({
        m_resources.gpu_resources.initial_spectrum_texture->image_view->bindless_index,
        m_resources.gpu_resources.angular_frequency_texture->image_view->bindless_index,
        m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index,
        m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index,
        m_resources.options.size,
        m_resources.data.total_time },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(tex_dispatch_size_xy, tex_dispatch_size_xy, m_resources.options.cascade_count);
    cmd->end_debug_region();

    x_y_z_xdx_barrier_info.stage_before
        = ydx_zdx_ydy_zdy_barrier_info.stage_before
        = rhi::Barrier_Pipeline_Stage::Compute_Shader;
    x_y_z_xdx_barrier_info.access_before
        = ydx_zdx_ydy_zdy_barrier_info.access_before
        = rhi::Barrier_Access::Unordered_Access_Write;
    x_y_z_xdx_barrier_info.access_after
        = ydx_zdx_ydy_zdy_barrier_info.access_after
        = rhi::Barrier_Access::Unordered_Access_Read;
    x_y_z_xdx_barrier_info.layout_before
        = ydx_zdx_ydy_zdy_barrier_info.layout_before
        = rhi::Barrier_Image_Layout::Unordered_Access;

    {
        auto image_barriers = std::to_array({
            x_y_z_xdx_barrier_info,
            ydx_zdx_ydy_zdy_barrier_info
            });
        rhi::Barrier_Info barrier = { .image_barriers = image_barriers };
        cmd->barrier(barrier);
    }

    cmd->begin_debug_region("IFFT", 0.25f, 0.5f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.fft_pipeline);
    cmd->add_debug_marker("IFFT-Vertical", 0.25f, 0.5f, 1.0f);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index, FFT_VERTICAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index, FFT_VERTICAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);

    {
        auto image_barriers = std::to_array({
            x_y_z_xdx_barrier_info,
            ydx_zdx_ydy_zdy_barrier_info
            });
        rhi::Barrier_Info barrier = { .image_barriers = image_barriers };
        cmd->barrier(barrier);
    }

    cmd->add_debug_marker("IFFT-Horizontal", 0.25f, 0.5f, 1.0f);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index, FFT_HORIZONTAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index, FFT_HORIZONTAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);
    cmd->end_debug_region();

    x_y_z_xdx_barrier_info.stage_after
        = ydx_zdx_ydy_zdy_barrier_info.stage_after
        = rhi::Barrier_Pipeline_Stage::Vertex_Shader;
    x_y_z_xdx_barrier_info.access_after
        = ydx_zdx_ydy_zdy_barrier_info.access_after
        = rhi::Barrier_Access::Shader_Read;
    x_y_z_xdx_barrier_info.layout_after
        = ydx_zdx_ydy_zdy_barrier_info.layout_after
        = rhi::Barrier_Image_Layout::Shader_Read_Only;

    {
        auto image_barriers = std::to_array({
            x_y_z_xdx_barrier_info,
            ydx_zdx_ydy_zdy_barrier_info
            });
        rhi::Barrier_Info barrier = { .image_barriers = image_barriers };
        cmd->barrier(barrier);
    }

    cmd->end_debug_region();
}

void Ocean_Renderer::render(rhi::Command_List* cmd) noexcept
{

    cmd->draw_indexed(0,1,0,0,0);
}

void Ocean_Renderer::render_patch(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept
{
    cmd->add_debug_marker("Ocean Render - Single Patch", .2f, .2f, 1.f);
    cmd->set_pipeline(m_resources.gpu_resources.render_patch_pipeline);
    constexpr auto SIZE = 2048;
    cmd->set_push_constants<Ocean_Render_Patch_Push_Constants>({
        .length_scales = m_resources.data.initial_spectrum_data.length_scales,
        .tex_sampler = m_resources.gpu_resources.linear_sampler->bindless_index,
        .camera = camera_buffer_bindless_index,
        .x_y_z_xdx_tex = m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index,
        .ydx_zdx_ydy_zdy_tex = m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index,
        .vertex_position_dist = .25f,
        .field_size = SIZE
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(6 * SIZE * SIZE, 1, 0, 0);
}

void Ocean_Renderer::render_composite(rhi::Command_List* cmd, uint32_t color_image_bindless_index) noexcept
{
    cmd->set_pipeline(m_resources.gpu_resources.render_composite_pipeline);
    cmd->set_push_constants<Ocean_Render_Composition_Push_Constants>({
        .rt_color_tex = color_image_bindless_index,
        .tex_sampler = m_resources.gpu_resources.linear_sampler->bindless_index
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(3,1,0,0);
}

void Ocean_Renderer::debug_render_slope(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept
{
    if (!m_resources.data.debug_render_slope) return;

    cmd->add_debug_marker("Ocean Debug Render - Slopes", .2f, .2f, 1.f);
    cmd->set_pipeline(m_resources.gpu_resources.debug_render_slopes_pipeline);
    constexpr auto SIZE = 32;
    cmd->set_push_constants<Ocean_Render_Debug_Push_Constants>({
        .length_scales =       m_resources.data.initial_spectrum_data.length_scales,
        .tex_sampler =         m_resources.gpu_resources.linear_sampler->bindless_index,
        .camera =              camera_buffer_bindless_index,
        .x_y_z_xdx_tex =       m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index,
        .ydx_zdx_ydy_zdy_tex = m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index,
        .line_scale =          .5f,
        .point_dist =          1.f,
        .point_field_size =    SIZE
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(2 * SIZE * SIZE, 1, 0, 0);
}

void Ocean_Renderer::debug_render_normal(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept
{
    if (!m_resources.data.debug_render_normal) return;

    cmd->add_debug_marker("Ocean Debug Render - Normals", .2f, .2f, 1.f);
    cmd->set_pipeline(m_resources.gpu_resources.debug_render_normals_pipeline);
    constexpr auto SIZE = 32;
    cmd->set_push_constants<Ocean_Render_Debug_Push_Constants>({
        .length_scales = m_resources.data.initial_spectrum_data.length_scales,
        .tex_sampler = m_resources.gpu_resources.linear_sampler->bindless_index,
        .camera = camera_buffer_bindless_index,
        .x_y_z_xdx_tex = m_resources.gpu_resources.x_y_z_xdx_texture->image_view->bindless_index,
        .ydx_zdx_ydy_zdy_tex = m_resources.gpu_resources.ydx_zdx_ydy_zdy_texture->image_view->bindless_index,
        .line_scale = 1.f,
        .point_dist = 1.f,
        .point_field_size = SIZE
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(2 * SIZE * SIZE, 1, 0, 0);
}

}
