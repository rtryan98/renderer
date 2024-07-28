#include "renderer/renderer.hpp"

#include "renderer/application.hpp"
#include "renderer/asset_manager.hpp"

namespace ren
{
Renderer::Renderer(Application& app, Asset_Manager& asset_manager)
    : m_app(app)
    , m_asset_manager(asset_manager)
{
    init_g_buffer();
}

void Renderer::init_g_buffer()
{
    Render_Attachment_Create_Info default_info = {
        .format = rhi::Image_Format::Undefined,
        .scaling_mode = Render_Attachment_Scaling_Mode::Ratio,
        .scaling_factor = m_render_scale,
        .layers = 1,
        .create_srv = false
    };

    auto target0_info = default_info;
    target0_info.name = "gbuffer0";
    target0_info.format = rhi::Image_Format::R16G16B16A16_SFLOAT;
    target0_info.create_srv = true;
    auto ds_info = default_info;
    ds_info.name = "gbuffer_ds";
    ds_info.format = rhi::Image_Format::D32_SFLOAT;

    m_g_buffer = {
        .target0 = m_asset_manager.create_render_attachment(target0_info),
        .depth_stencil = m_asset_manager.create_render_attachment(ds_info)
    };
}
}
