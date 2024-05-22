#include "renderer/ocean/ocean_renderer.hpp"

#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"

namespace ren
{
Ocean_Renderer::Ocean_Renderer(Asset_Manager& asset_manager, Shader_Library& shader_library)
    : m_asset_manager(asset_manager)
    , m_shader_library(shader_library)
    , m_resources()
    , m_settings(m_resources, m_asset_manager, m_shader_library)
{
    m_resources.create_textures(m_asset_manager);
    m_resources.gpu_resources.fft_pipeline = m_asset_manager.create_pipeline({
        .cs = m_shader_library.get_shader(
            select_fft_shader(
                m_resources.options.size,
                m_resources.options.use_fp16_maths,
                true))});
}

Ocean_Renderer::~Ocean_Renderer()
{
    m_asset_manager.destroy_image(m_resources.gpu_resources.spectrum_texture);

    m_asset_manager.destroy_pipeline(m_resources.gpu_resources.fft_pipeline);
}

Ocean_Settings* Ocean_Renderer::get_settings() noexcept
{
    return &m_settings;
}
}
