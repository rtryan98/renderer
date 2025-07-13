#include "renderer/application.hpp"

#include <chrono>
#include <filesystem>
#include <imgui.h>

namespace ren
{
Application::Application() noexcept
    : m_logger(std::make_shared<Logger>())
    , m_asset_path(init_asset_path())
    , m_window(Window::create({
        .width = 2560,
        .height = 1440,
        .title = "Renderer",
        .dpi_aware_size = false
        }))
    , m_input_state(std::make_unique<Input_State>(*m_window))
    , m_device(rhi::Graphics_Device::create({
        .graphics_api = rhi::Graphics_API::D3D12,
        .enable_validation = true,
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
    , m_staging_buffers()
    , m_buffer_staging_infos()
    , m_frame_counter(0)
    , m_asset_repository(std::make_unique<Asset_Repository>(
        m_logger,
        m_device.get(),
        Asset_Repository_Paths {
        .shaders = m_asset_path + "/shaders/",
        .pipelines = m_asset_path + "/pipelines/",
        .shader_include_paths = {
            "../",
            "../../src/shared/",
            "../../thirdparty/rhi/src/shaders/"
        },
        .models = m_asset_path + "/cache/"},
        *this))
    , m_resource_blackboard(std::make_unique<Render_Resource_Blackboard>(m_device.get()))
    , m_renderer(*this, *m_swapchain, *m_resource_blackboard, Imgui_Renderer_Create_Info{
        .device = m_device.get(),
        .frames_in_flight = FRAME_IN_FLIGHT_COUNT,
        .swapchain_image_format = m_swapchain->get_image_format()})
    , m_is_running(true)
    , m_cbt_cpu_vis(nullptr)
    , m_renderer_settings()
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

    auto renderer_settings = m_renderer.get_settings();
    for (auto settings : renderer_settings)
    {
        m_renderer_settings.add_settings(settings);
    }
    imgui_setup_style();

    m_logger->info("Finished initializing.");
}

Application::~Application() noexcept
{
    m_logger->info("Shutting down.");
    m_device->wait_idle();
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
        m_input_state->update();

        delta_time = std::chrono::duration_cast<std::chrono::duration<double>>(
            current_time - last_time)
            .count();
        total_time += delta_time;

        auto& frame = m_frames[m_frame_counter % FRAME_IN_FLIGHT_COUNT];

        setup_frame(frame);
        process_gui();
        update(total_time, delta_time);
        render_frame(frame, total_time, delta_time);

        if (!m_window->get_window_data().is_alive)
        {
            m_is_running = false;
        }

        last_time = current_time;
        current_time = std::chrono::steady_clock::now();

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

void Application::upload_image_data_immediate_full(rhi::Image* image, void** data) noexcept
{
    const auto byte_size = rhi::get_image_format_info(image->format).bytes;
    std::size_t size = 0;
    for (auto i = 0; i < image->mip_levels; ++i)
    {
        size += byte_size * (image->width / (1 << i)) * (image->height / (1 << i));
    }

    auto& staging_buffers = m_staging_buffers[m_frame_counter % FRAME_IN_FLIGHT_COUNT];
    rhi::Buffer_Create_Info buffer_info = {
        .size = std::max(4ull, size),
        .heap = rhi::Memory_Heap_Type::CPU_Upload
    };
    auto staging_buffer = m_device->create_buffer(buffer_info).value_or(nullptr);

    std::size_t offset = 0;
    for (auto i = 0; i < image->mip_levels; ++i)
    {
        std::size_t current_size = byte_size * (image->width / (1 << i)) * (image->height / (1 << i));
        if (i > 0)
        {
            offset += byte_size * (image->width / (1 << (i - 1))) * (image->height / (1 << (i - 1)));
        }
        memcpy(&static_cast<char*>(staging_buffer->data)[offset], data[i], current_size);
    }

    staging_buffers.push_back(staging_buffer);
    m_image_staging_infos[m_frame_counter % FRAME_IN_FLIGHT_COUNT].push_back({
        .src = staging_buffer,
        .dst = image });
}

std::string Application::init_asset_path() const
{
    auto dir = std::filesystem::path();
    for (uint32_t back_search = 0; back_search < 5; ++back_search)
    {
        if (auto path_marker = dir / "assets" / "meta"; std::filesystem::exists(path_marker))
            break;
        dir = dir / "..";
    }
    auto prefix = (dir / "assets").string();
    m_logger->info("Asset path is '{}'.", prefix);
    return prefix;
}

void Application::setup_frame(Frame& frame) noexcept
{
    frame.frame_fence->wait_for_value(frame.fence_value);
    frame.graphics_command_pool->reset();
    frame.compute_command_pool->reset();
    frame.copy_command_pool->reset();
    m_resource_blackboard->garbage_collect(m_frame_counter);
    auto [is_size_changed, width, height] = m_swapchain->query_resize();
    if (is_size_changed)
    {
        m_renderer.on_resize(width, height);
    }
    m_swapchain->acquire_next_image();

    if (m_frame_counter > FRAME_IN_FLIGHT_COUNT)
    {
        auto last_frame_in_flight = (m_frame_counter - FRAME_IN_FLIGHT_COUNT) % FRAME_IN_FLIGHT_COUNT;
        m_buffer_staging_infos[last_frame_in_flight].clear();
        m_image_staging_infos[last_frame_in_flight].clear();
        for (auto staging_buffer : m_staging_buffers[last_frame_in_flight])
        {
            m_device->destroy_buffer(staging_buffer);
        }
        m_staging_buffers[last_frame_in_flight].clear();
    }

    m_renderer.setup_frame();
}

void Application::render_frame(Frame& frame, double t, double dt) noexcept
{
    auto graphics_cmd = frame.graphics_command_pool->acquire_command_list();

    m_renderer.render(graphics_cmd, t, dt);

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
    const auto upload_cmd = frame.graphics_command_pool->acquire_command_list();
    const auto frame_idx = m_frame_counter % FRAME_IN_FLIGHT_COUNT;
    std::vector<rhi::Image_Barrier_Info> image_barriers_before;
    std::vector<rhi::Image_Barrier_Info> image_barriers_after;
    image_barriers_before.reserve(m_image_staging_infos[frame_idx].size());
    image_barriers_after.reserve(m_image_staging_infos[frame_idx].size());
    for (const auto& image_staging_info : m_image_staging_infos[frame_idx])
    {
        image_barriers_before.emplace_back( rhi::Image_Barrier_Info {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Copy,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Transfer_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Copy_Dst,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = image_staging_info.dst,
            .subresource_range = {
                .first_mip_level = 0,
                .mip_count = image_staging_info.dst->mip_levels,
                .first_array_index = 0,
                .array_size = image_staging_info.dst->array_size,
                .first_plane = 0,
                .plane_count = 1
            },
            .discard = true
        });
        image_barriers_after.emplace_back( rhi::Image_Barrier_Info {
            .stage_before = rhi::Barrier_Pipeline_Stage::Copy,
            .stage_after = rhi::Barrier_Pipeline_Stage::All_Commands,
            .access_before = rhi::Barrier_Access::Transfer_Write,
            .access_after = rhi::Barrier_Access::Shader_Read,
            .layout_before = rhi::Barrier_Image_Layout::Copy_Dst,
            .layout_after = rhi::Barrier_Image_Layout::Shader_Read_Only,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = image_staging_info.dst,
            .subresource_range = {
                .first_mip_level = 0,
                .mip_count = image_staging_info.dst->mip_levels,
                .first_array_index = 0,
                .array_size = image_staging_info.dst->array_size,
                .first_plane = 0,
                .plane_count = 1
            },
            .discard = false
        });
    }
    if (image_barriers_before.size() > 0)
    {
        upload_cmd->barrier({
            .image_barriers = image_barriers_before
            });
    }

    // TODO: this is temporary and will be reworked when uploads are moved to the copy queue
    for (const auto& image_staging_info : m_image_staging_infos[frame_idx])
    {
        std::size_t offset = 0;
        for (auto i = 0; i < image_staging_info.dst->mip_levels; ++i)
        {
            const auto byte_size = rhi::get_image_format_info(image_staging_info.dst->format).bytes;
            const auto width = image_staging_info.dst->width / (1 << i);
            const auto height = image_staging_info.dst->height / (1 << i);
            if (i > 0)
            {
                offset += byte_size * (image_staging_info.dst->width / (1 << (i - 1))) * (image_staging_info.dst->height / (1 << (i - 1)));
            }
            upload_cmd->copy_buffer_to_image(
                image_staging_info.src,
                offset,
                image_staging_info.dst,
                {},
                {
                .x = width,
                .y = height,
                .z = 1
                },
                i,
                0);
        }
    }

    for (const auto& buffer_staging_info : m_buffer_staging_infos[frame_idx])
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
        .image_barriers = image_barriers_after,
        .memory_barriers = { &mem_barrier, 1 },
    };
    upload_cmd->barrier(barrier);
    return upload_cmd;
}

void Application::process_gui() noexcept
{
    ImGui::NewFrame();

    m_renderer.overlay_gui();

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

void Application::update(double t, double dt) noexcept
{
    m_renderer.update(*m_input_state, t, dt);
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

    style.ScaleAllSizes(m_window->get_dpi_scale());
    ImGui::GetIO().FontGlobalScale = m_window->get_dpi_scale();
    Settings_Base::set_pad_scale(m_window->get_dpi_scale());
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
