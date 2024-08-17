#include "renderer/renderer.hpp"

#include "renderer/application.hpp"
#include "renderer/asset_manager.hpp"

#include "renderer/window.hpp"
#include "renderer/input_codes.hpp"

#include "shared/camera_shared_types.h"

#undef near
#undef far

namespace ren
{
float calculate_aspect_ratio(const Window& window)
{
    auto& window_data = window.get_window_data();
    return float(window_data.width) / float(window_data.height);
}

Renderer::Renderer(Application& app, Asset_Manager& asset_manager,
    Shader_Library& shader_library, rhi::Swapchain& swapchain,
    const Imgui_Renderer_Create_Info& imgui_renderer_create_info)
    : m_app(app)
    , m_asset_manager(asset_manager)
    , m_shader_library(shader_library)
    , m_swapchain(swapchain)
    , m_fly_cam{
        .fov_y = 90.f,
        .aspect = calculate_aspect_ratio(app.get_window()),
        .near = .01f,
        .far = 1000.f,
        .sensitivity = .25f,
        .movement_speed = 10.f,
        .pitch = 0.f,
        .yaw = 0.f,
        .position = { .0f, .0f, .5f }
    }
    , m_camera_buffer(m_asset_manager.create_buffer({
        .size = sizeof(GPU_Camera_Data),
        .heap = rhi::Memory_Heap_Type::GPU
        }, "Camera Buffer"))
    , m_imgui_renderer(imgui_renderer_create_info)
    , m_ocean_renderer(m_asset_manager, m_shader_library)
{
    init_g_buffer();
    m_imgui_renderer.create_fonts_texture();
}

std::vector<Settings_Base*> Renderer::get_settings() noexcept
{
    std::vector<Settings_Base*> result;
    result.push_back(m_ocean_renderer.get_settings());
    return result;
}

void Renderer::update(const Input_State& input_state, double t, double dt) noexcept
{
    if (!(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard))
    {
        // No input captured here from UI

        m_fly_cam.process_inputs(input_state, float(dt));
    }
    // set aspect ratio in case of resize
    m_fly_cam.aspect = calculate_aspect_ratio(m_app.get_window());
    m_fly_cam.update();
}

void Renderer::overlay_gui()
{
    {
        ImGui::SetNextWindowPos({ 50.f, 50.f });
        ImGui::SetNextWindowSizeConstraints({ 500.f, 500.f }, { 9999.f, 9999.f });
        ImGui::Begin("##InvisibleCameraWindow", nullptr,
              ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove);
        ImGui::Text("Camera Position: %.3f, %.3f, %.3f",
            m_fly_cam.position.x,
            m_fly_cam.position.y,
            m_fly_cam.position.z);
        ImGui::Text("Camera Direction: %.3f, %.3f, %.3f",
            m_fly_cam.forward.x,
            m_fly_cam.forward.y,
            m_fly_cam.forward.z);
        ImGui::End();
    }
}

void Renderer::setup_frame()
{
    m_imgui_renderer.next_frame();
}

void Renderer::render(rhi::Command_List* cmd, double t, double dt) noexcept
{
    GPU_Camera_Data camera_data = {
        .view = m_fly_cam.camera_data.view,
        .proj = m_fly_cam.camera_data.proj,
        .view_proj = m_fly_cam.camera_data.view_proj,
        .position = m_fly_cam.camera_data.position
    };
    m_app.upload_buffer_data_immediate(m_camera_buffer, &camera_data, sizeof(camera_data), 0);

    m_ocean_renderer.simulate(m_app, cmd, dt);

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

    auto& window_data = m_app.get_window().get_window_data();
    auto scale = m_app.get_window().get_dpi_scale();
    cmd->begin_render_pass(render_pass_info);
    cmd->set_viewport(0.f, 0.f, float(window_data.width) * scale, float(window_data.height) * scale, 0.f, 1.f);
    cmd->set_scissor(0, 0, window_data.width * scale, window_data.height * scale);
    cmd->begin_debug_region("Debug Renderer", 1.f, .75f, .75f);
    m_ocean_renderer.debug_render_slope(cmd, m_camera_buffer->buffer_view->bindless_index);
    cmd->end_debug_region();
    cmd->add_debug_marker("Dear ImGui", .5f, 1.f, .0f);
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
