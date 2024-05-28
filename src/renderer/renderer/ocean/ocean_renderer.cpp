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

    cmd->set_pipeline(m_resources.gpu_resources.fft_pipeline);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.spectrum_texture->image_view->bindless_index, FFT_VERTICAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(m_resources.options.size, 1, m_resources.options.cascade_count);

    image_barrier.stage_before = rhi::Barrier_Pipeline_Stage::Compute_Shader;
    image_barrier.access_before = rhi::Barrier_Access::Shader_Write;
    image_barrier.access_after = rhi::Barrier_Access::Shader_Read;
    image_barrier.layout_before = rhi::Barrier_Image_Layout::General;
    cmd->barrier(barrier);

    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.spectrum_texture->image_view->bindless_index, FFT_HORIZONTAL, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(m_resources.options.size, 1, m_resources.options.cascade_count);

    cmd->barrier(barrier);
}
}
