#include "renderer/ocean/ocean_renderer.hpp"

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
    m_resources.create_textures(m_asset_manager);
    m_resources.create_pipelines(m_asset_manager, m_shader_library);
}

Ocean_Renderer::~Ocean_Renderer()
{
    m_resources.destroy_textures(m_asset_manager);
    m_resources.destroy_pipelines(m_asset_manager);
}

Ocean_Settings* Ocean_Renderer::get_settings() noexcept
{
    return &m_settings;
}

void Ocean_Renderer::simulate(Application& app, rhi::Command_List* cmd) noexcept
{
    cmd->begin_debug_region("Ocean Simulation", 0.5f, 0.5f, 1.f);

    rhi::Image_Barrier_Info image_barrier = {
        .stage_before = rhi::Barrier_Pipeline_Stage::None,
        .stage_after = rhi::Barrier_Pipeline_Stage::Compute_Shader,
        .access_before = rhi::Barrier_Access::None,
        .access_after = rhi::Barrier_Access::Shader_Write,
        .layout_before = rhi::Barrier_Image_Layout::Undefined,
        .layout_after = rhi::Barrier_Image_Layout::General,
        .image = m_resources.gpu_resources.spectrum_texture,
        .subresource_range = {
            .first_mip_level = 0,
            .mip_count = m_resources.gpu_resources.spectrum_texture->mip_levels,
            .first_array_index = 0,
            .array_size = m_resources.gpu_resources.spectrum_texture->array_size,
            .first_plane = 0,
            .plane_count = 1
        },
        .discard = true
    };
    rhi::Barrier_Info barrier = { .image_barriers = { &image_barrier, 1 } };

    cmd->barrier(barrier);

    cmd->begin_debug_region("Initial Spectrum", 0.25f, 0.0f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.initial_spectrum_pipeline);
    uint32_t tex_dispatch_size_xy =
        m_resources.options.size /
        m_resources.gpu_resources.initial_spectrum_pipeline->compute_shading_info.cs->groups_x;
    cmd->set_push_constants<Ocean_Initial_Spectrum_Push_Constants>({
        0,
        m_resources.gpu_resources.spectrum_texture->image_view->bindless_index },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(tex_dispatch_size_xy, tex_dispatch_size_xy, m_resources.options.cascade_count);
    cmd->end_debug_region();

    image_barrier.stage_before = rhi::Barrier_Pipeline_Stage::Compute_Shader;
    image_barrier.access_before = rhi::Barrier_Access::Shader_Write;
    image_barrier.access_after = rhi::Barrier_Access::Shader_Read;
    image_barrier.layout_before = rhi::Barrier_Image_Layout::General;
    cmd->barrier(barrier);

    cmd->begin_debug_region("Time Dependent Spectrum", 0.25f, 0.25f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.time_dependent_spectrum_pipeline);
    cmd->set_push_constants<Ocean_Time_Dependent_Spectrum_Push_Constants>({
        m_resources.gpu_resources.spectrum_texture->image_view->bindless_index },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(tex_dispatch_size_xy, tex_dispatch_size_xy, m_resources.options.cascade_count);
    cmd->end_debug_region();

    cmd->barrier(barrier);

    cmd->begin_debug_region("IFFT", 0.25f, 0.5f, 1.0f);
    cmd->set_pipeline(m_resources.gpu_resources.fft_pipeline);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.spectrum_texture->image_view->bindless_index, FFT_VERTICAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->add_debug_marker("IFFT-Vertical", 0.25f, 0.5f, 1.0f);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);
    cmd->barrier(barrier);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.spectrum_texture->image_view->bindless_index, FFT_HORIZONTAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->add_debug_marker("IFFT-Horizontal", 0.25f, 0.5f, 1.0f);
    cmd->dispatch(1, m_resources.options.size, m_resources.options.cascade_count);
    cmd->end_debug_region();

    image_barrier.stage_after = rhi::Barrier_Pipeline_Stage::Vertex_Shader;
    image_barrier.layout_after = rhi::Barrier_Image_Layout::Shader_Read_Only;
    cmd->barrier(barrier);

    cmd->end_debug_region();
}
}
