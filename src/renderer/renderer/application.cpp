#include "renderer/application.hpp"

#include <chrono>
#include <print>
#include <imgui.h>

namespace ren
{
Application::Application() noexcept
    : m_window(Window::create({
        .width = 1920,
        .height = 1080,
        .title = "Renderer"
        }))
    , m_device(rhi::Graphics_Device::create({
        .graphics_api = rhi::Graphics_API::D3D12,
        .enable_validation = false,
        .enable_gpu_validation = false,
        .enable_locking = false
        }))
    , m_swapchain(m_device->create_swapchain({
        .hwnd = m_window->get_native_handle(),
        .preferred_format = rhi::Image_Format::R8G8B8A8_UNORM,
        .image_count = FRAME_IN_FLIGHT_COUNT + 1,
        .present_mode = rhi::Present_Mode::Immediate
        }))
    , m_frames()
    , m_frame_counter(0)
    , m_is_running(true)
    , m_imgui_renderer(std::make_unique<Imgui_Renderer>(Imgui_Renderer_Create_Info{
        .device = m_device.get(),
        .frames_in_flight = FRAME_IN_FLIGHT_COUNT,
        .swapchain_image_format = m_swapchain->get_image_format()
        }))
{
    for (auto& frame : m_frames)
    {
        auto frame_fence = m_device->create_fence(0);
        frame.frame_fence = frame_fence.value_or(nullptr);
        if (!frame.frame_fence) std::print("Failed to create frame fence!");
        frame.fence_value = 0ull;
        frame.graphics_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Graphics });
        frame.compute_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Compute });
        frame.copy_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Copy });
    }
    m_imgui_renderer->create_fonts_texture();
}

Application::~Application() noexcept
{
    m_device->wait_idle();
    for (auto& frame : m_frames)
    {
        m_device->destroy_fence(frame.frame_fence);
    }
}

void Application::run()
{
    auto current_time = std::chrono::steady_clock::now();
    auto last_time = std::chrono::steady_clock::now();
    double delta_time = 0.0;
    double total_time = 0.0;

    while (m_is_running)
    {
        m_window->update();

        delta_time = std::chrono::duration_cast<std::chrono::duration<double>>(
            current_time - last_time)
            .count();
        total_time += delta_time;

        auto& frame = m_frames[m_frame_counter % FRAME_IN_FLIGHT_COUNT];
        setup_frame(frame);
        ImGui::NewFrame();

        bool demo = true;
        ImGui::ShowDemoWindow(&demo);

        render_frame(frame, total_time, delta_time);
        ImGui::EndFrame();

        if (!m_window->get_window_data().is_alive)
        {
            m_is_running = false;
        }

        last_time = current_time;
        current_time = std::chrono::steady_clock::now();
    }
}

void Application::setup_frame(Frame& frame) noexcept
{
    frame.frame_fence->wait_for_value(frame.fence_value);
    frame.graphics_command_pool->reset();
    frame.compute_command_pool->reset();
    frame.copy_command_pool->reset();
    auto swapchain_resize = m_swapchain->query_resize();
    if (swapchain_resize.is_size_changed)
    {
        // Resize window size dependent resources
    }
    m_swapchain->acquire_next_image();
    m_imgui_renderer->next_frame();
}

void Application::render_frame(Frame& frame, double t, double dt) noexcept
{
    auto swapchain_image_view = m_swapchain->get_current_image_view();
    auto graphics_cmd = frame.graphics_command_pool->acquire_command_list();

    graphics_cmd->begin_debug_region("PASS-Swapchain", 0.5f, 0.25f, 0.25f);
    rhi::Image_Barrier_Info swapchain_layout_transition_barrier = {
        .stage_before = rhi::Barrier_Pipeline_Stage::None,
        .stage_after = rhi::Barrier_Pipeline_Stage::Clear,
        .access_before = rhi::Barrier_Access::None,
        .access_after = rhi::Barrier_Access::Color_Attachment_Write,
        .layout_before = rhi::Barrier_Image_Layout::Undefined,
        .layout_after = rhi::Barrier_Image_Layout::Color_Attachment,
        .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
        .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
        .image = swapchain_image_view->image,
        .subresource_range = {
            .first_mip_level = 0,
            .mip_count = 1,
            .first_array_index = 0,
            .array_size = 1,
            .first_plane = 0,
            .plane_count = 1
        },
        .discard = true
    };
    graphics_cmd->barrier({
        .buffer_barriers = {},
        .image_barriers = { &swapchain_layout_transition_barrier, 1 },
        .memory_barriers = {}
        });

    graphics_cmd->clear_color_attachment(
        swapchain_image_view,
        0.0f, 0.0f, 0.0f, 0.0f);
    rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_image_view, 1 },
        .depth_attachment = nullptr
    };

    graphics_cmd->begin_render_pass(render_pass_info);
    m_imgui_renderer->render(graphics_cmd);
    graphics_cmd->end_render_pass();

    swapchain_layout_transition_barrier.stage_before = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output;
    swapchain_layout_transition_barrier.stage_after = rhi::Barrier_Pipeline_Stage::None;
    swapchain_layout_transition_barrier.access_before = rhi::Barrier_Access::Color_Attachment_Write;
    swapchain_layout_transition_barrier.access_after = rhi::Barrier_Access::None;
    swapchain_layout_transition_barrier.layout_before = rhi::Barrier_Image_Layout::Color_Attachment;
    swapchain_layout_transition_barrier.layout_after = rhi::Barrier_Image_Layout::Present;
    swapchain_layout_transition_barrier.discard = false;
    graphics_cmd->barrier({
        .buffer_barriers = {},
        .image_barriers = { &swapchain_layout_transition_barrier, 1 },
        .memory_barriers = {}
        });
    graphics_cmd->end_debug_region();

    frame.fence_value += 1;
    rhi::Submit_Fence_Info frame_fence_signal_info = {
        .fence = frame.frame_fence,
        .value = frame.fence_value
    };
    m_device->submit({
        .queue_type = rhi::Queue_Type::Graphics,
        .wait_swapchain = m_swapchain.get(),
        .present_swapchain = m_swapchain.get(),
        .wait_infos = {},
        .command_lists = { &graphics_cmd, 1 },
        .signal_infos = { &frame_fence_signal_info, 1 }
        });
    m_swapchain->present();
}
}
