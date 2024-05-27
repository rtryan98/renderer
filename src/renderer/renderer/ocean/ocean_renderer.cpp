#include "renderer/ocean/ocean_renderer.hpp"

#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"

#include <rhi/command_list.hpp>
#include <shaders/fft_shared_types.hlsli>
#include <shaders/ocean/ocean_shared_types.hlsli>

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
    cmd->set_pipeline(m_resources.gpu_resources.fft_pipeline);
    cmd->set_push_constants<FFT_Push_Constants>(
        { m_resources.gpu_resources.spectrum_texture->image_view->bindless_index, true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(m_resources.options.size, 1, 1);
}
}
