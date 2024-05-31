#include "renderer/application.hpp"

#include <chrono>
#include <imgui.h>

namespace ren
{
Application::Application() noexcept
    : m_logger(std::make_shared<Logger>())
    , m_window(Window::create({
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
    , m_shader_library(m_logger, m_device.get())
    , m_asset_manager(m_logger, m_device.get(), 2)
    , m_frames()
    , m_staging_buffers()
    , m_frame_counter(0)
    , m_is_running(true)
    , m_imgui_renderer(std::make_unique<Imgui_Renderer>(Imgui_Renderer_Create_Info{
        .device = m_device.get(),
        .frames_in_flight = FRAME_IN_FLIGHT_COUNT,
        .swapchain_image_format = m_swapchain->get_image_format()
        }))
    , m_cbt_cpu_vis(nullptr)
    , m_renderer_settings()
    , m_ocean_renderer(std::make_unique<Ocean_Renderer>(m_asset_manager, m_shader_library))
{
    for (auto& frame : m_frames)
    {
        auto frame_fence = m_device->create_fence(0);
        frame.frame_fence = frame_fence.value_or(nullptr);
        if (!frame.frame_fence)
        {
            m_logger->critical("Failed to create frame fence!");
            std::abort();
        }
        frame.fence_value = 0ull;
        frame.graphics_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Graphics });
        frame.compute_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Compute });
        frame.copy_command_pool = m_device->create_command_pool({ .queue_type = rhi::Queue_Type::Copy });
    }
    imgui_setup_style();
    m_imgui_renderer->create_fonts_texture();

    m_renderer_settings.add_settings(m_ocean_renderer->get_settings());

    m_logger->info("Finished initializing.");
}

Application::~Application() noexcept
{
    m_logger->info("Shutting down.");
    m_device->wait_idle();
    m_ocean_renderer = nullptr;
    m_asset_manager.flush_deletion_queue(~0ull);
    for (auto& frame : m_frames)
    {
        m_device->destroy_fence(frame.frame_fence);
    }
    for (auto& staging_buffer_vec : m_staging_buffers)
    {
        for (auto staging_buffer : staging_buffer_vec)
        {
            m_device->destroy_buffer(staging_buffer);
        }
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
        process_gui();
        render_frame(frame, total_time, delta_time);

        if (!m_window->get_window_data().is_alive)
        {
            m_is_running = false;
        }

        last_time = current_time;
        current_time = std::chrono::steady_clock::now();

        m_asset_manager.next_frame();
        m_frame_counter += 1;
    }
}

void Application::upload_buffer_data_immediate(rhi::Buffer* buffer, void* data, uint64_t size, uint64_t offset) noexcept
{
    auto& staging_buffers = m_staging_buffers[m_frame_counter % FRAME_IN_FLIGHT_COUNT];
    rhi::Buffer_Create_Info buffer_info = {
        .size = size,
        .heap = rhi::Memory_Heap_Type::CPU_Upload
    };
    auto staging_buffer = m_device->create_buffer(buffer_info).value_or(nullptr);
    memcpy(staging_buffer->data, data, size);

    staging_buffers.push_back(staging_buffer);
    m_buffer_staging_infos[m_frame_counter % FRAME_IN_FLIGHT_COUNT].push_back({
        .src = staging_buffer,
        .dst = buffer,
        .offset = offset,
        .size = size });
}

void Application::setup_frame(Frame& frame) noexcept
{
    frame.frame_fence->wait_for_value(frame.fence_value);
    frame.graphics_command_pool->reset();
    frame.compute_command_pool->reset();
    frame.copy_command_pool->reset();
    m_asset_manager.flush_deletion_queue(m_frame_counter);
    auto swapchain_resize = m_swapchain->query_resize();
    if (swapchain_resize.is_size_changed)
    {
        // Resize window size dependent resources
    }
    m_swapchain->acquire_next_image();
    m_imgui_renderer->next_frame();

    if (m_frame_counter > FRAME_IN_FLIGHT_COUNT)
    {
        auto last_frame_in_flight = (m_frame_counter - FRAME_IN_FLIGHT_COUNT) % FRAME_IN_FLIGHT_COUNT;
        m_buffer_staging_infos[last_frame_in_flight].clear();
        for (auto staging_buffer : m_staging_buffers[last_frame_in_flight])
        {
            m_device->destroy_buffer(staging_buffer);
        }
        m_staging_buffers[last_frame_in_flight].clear();
    }
}

void Application::render_frame(Frame& frame, double t, double dt) noexcept
{
    auto swapchain_image_view = m_swapchain->get_current_image_view();
    auto graphics_cmd = frame.graphics_command_pool->acquire_command_list();

    m_ocean_renderer->simulate(*this, graphics_cmd);

    graphics_cmd->begin_debug_region("Swapchain Pass", 0.5f, 0.25f, 0.25f);
    rhi::Image_Barrier_Info swapchain_layout_transition_barrier = {
        .stage_before = rhi::Barrier_Pipeline_Stage::None,
        .stage_after = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
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

    auto cmds = std::to_array({ handle_immediate_uploads(frame), graphics_cmd });

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
        .command_lists = cmds,
        .signal_infos = { &frame_fence_signal_info, 1 }
        });
    m_swapchain->present();
}

rhi::Command_List* Application::handle_immediate_uploads(Frame& frame) noexcept
{
    auto upload_cmd = frame.graphics_command_pool->acquire_command_list();

    for (auto& buffer_staging_info : m_buffer_staging_infos[m_frame_counter % FRAME_IN_FLIGHT_COUNT])
    {
        upload_cmd->copy_buffer(
            buffer_staging_info.src,
            0,
            buffer_staging_info.dst,
            buffer_staging_info.offset,
            buffer_staging_info.size);
    }
    rhi::Memory_Barrier_Info mem_barrier = {
        .stage_before = rhi::Barrier_Pipeline_Stage::Copy,
        .stage_after = rhi::Barrier_Pipeline_Stage::All_Commands,
        .access_before = rhi::Barrier_Access::Transfer_Write,
        .access_after = rhi::Barrier_Access::Shader_Read
    };
    rhi::Barrier_Info barrier = {
        .memory_barriers = { &mem_barrier, 1 }
    };
    upload_cmd->barrier(barrier);
    return upload_cmd;
}

void Application::process_gui() noexcept
{
    ImGui::NewFrame();
    imgui_menubar();

    if (m_imgui_data.windows.renderer_settings)
        m_renderer_settings.process_gui(&m_imgui_data.windows.renderer_settings);
    if (m_imgui_data.windows.tool_cbt_vis)
    {
        // no need to allocate cpu-sided vis if it's not used
        if (!m_cbt_cpu_vis) m_cbt_cpu_vis = std::make_unique<CBT_CPU_Vis>();
        m_cbt_cpu_vis->imgui_window(m_imgui_data.windows.tool_cbt_vis);
    }
    if (m_imgui_data.windows.demo)
        ImGui::ShowDemoWindow(&m_imgui_data.windows.demo);
    ImGui::EndFrame();
}

void Application::imgui_close_all_windows() noexcept
{
    m_imgui_data.windows.demo = false;
    m_imgui_data.windows.renderer_settings = false;
    m_imgui_data.windows.tool_cbt_vis = false;
}

void Application::imgui_setup_style() noexcept
{
    // Deep Dark theme https://github.com/ocornut/imgui/issues/707#issuecomment-917151020
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}

void imgui_menu_toggle_window(const char* name, bool& window_open)
{
    if (ImGui::MenuItem(name, nullptr, window_open))
    {
        window_open = !window_open;
    }
}

void Application::imgui_menubar() noexcept
{
    auto& style = ImGui::GetStyle();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("Import Model..."))
            {

            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            imgui_menu_toggle_window("Renderer Settings", m_imgui_data.windows.renderer_settings);
            if (ImGui::MenuItem("Close all Windows"))
            {
                imgui_close_all_windows();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            imgui_menu_toggle_window("CBT and LEB Visualization", m_imgui_data.windows.tool_cbt_vis);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug"))
        {
            imgui_menu_toggle_window("ImGui Demo Window", m_imgui_data.windows.demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
}
