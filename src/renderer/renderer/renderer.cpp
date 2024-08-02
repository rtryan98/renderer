#include "renderer/renderer.hpp"

#include "renderer/application.hpp"
#include "renderer/asset_manager.hpp"

namespace ren
{
Renderer::Renderer(Application& app, Asset_Manager& asset_manager,
    rhi::Swapchain& swapchain,
    const Imgui_Renderer_Create_Info& imgui_renderer_create_info)
    : m_app(app)
    , m_asset_manager(asset_manager)
    , m_swapchain(swapchain)
    , m_imgui_renderer(imgui_renderer_create_info)
{
    init_g_buffer();
    m_imgui_renderer.create_fonts_texture();
}

void Renderer::update(double t, double dt) noexcept
{

}

void Renderer::setup_frame()
{
    m_imgui_renderer.next_frame();
}

void Renderer::render(rhi::Command_List* cmd) noexcept
{
    auto swapchain_image_view = m_swapchain.get_current_image_view();
    constexpr static rhi::Image_Barrier_Subresource_Range RT_DEFAULT_SUBRESOURCE_RANGE = {
        .first_mip_level = 0,
        .mip_count = 1,
        .first_array_index = 0,
        .array_size = 1,
        .first_plane = 0,
        .plane_count = 1
    };
    /* Transition Swapchain to Color Attachment layout */ {
        rhi::Image_Barrier_Info swapchain_layout_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Color_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Color_Attachment,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = swapchain_image_view->image,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = { &swapchain_layout_barrier, 1 },
            .memory_barriers = {}
            });
    }
    render_swapchain_pass(cmd);
    /* Transition Swapchain to Present layout */ {
        rhi::Image_Barrier_Info swapchain_layout_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .stage_after = rhi::Barrier_Pipeline_Stage::None,
            .access_before = rhi::Barrier_Access::Color_Attachment_Write,
            .access_after = rhi::Barrier_Access::None,
            .layout_before = rhi::Barrier_Image_Layout::Color_Attachment,
            .layout_after = rhi::Barrier_Image_Layout::Present,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = swapchain_image_view->image,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = { &swapchain_layout_barrier, 1 },
            .memory_barriers = {}
            });
    }
}

void Renderer::render_swapchain_pass(rhi::Command_List* cmd)
{
    auto swapchain_image_view = m_swapchain.get_current_image_view();

    cmd->begin_debug_region("Swapchain Pass", 0.5f, 0.25f, 0.25f);

    rhi::Render_Pass_Color_Attachment_Info swapchain_attachment_info = {
        .attachment = swapchain_image_view,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
        .clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };
    rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_attachment_info, 1 },
        .depth_stencil_attachment = {}
    };

    cmd->begin_render_pass(render_pass_info);
    m_imgui_renderer.render(cmd);
    cmd->end_render_pass();

    cmd->end_debug_region();
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
