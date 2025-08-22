#include "renderer/application.hpp"

#include <chrono>
#include <filesystem>
#include <imgui.h>

#include "imgui/imgui_util.hpp"

namespace ren
{
Application::Application(const Application_Create_Info& create_info) noexcept
    : m_logger(std::make_shared<Logger>())
    , m_window(Window::create({
        .width = create_info.width,
        .height = create_info.height,
        .title = "Renderer",
        .dpi_aware_size = false,
        .borderless = true
        }))
    , m_input_state(std::make_unique<Input_State>(*m_window))
    , m_device(rhi::Graphics_Device::create({
        .graphics_api = rhi::Graphics_API::D3D12,
        .enable_validation = create_info.enable_validation,
        .enable_gpu_validation = create_info.enable_gpu_validation,
        .enable_locking = true
        }))
    , m_swapchain(m_device->create_swapchain({
        .hwnd = m_window->get_native_handle(),
        .preferred_format = rhi::Image_Format::R8G8B8A8_UNORM, // SDR
        // .preferred_format = rhi::Image_Format::A2R10G10B10_UNORM_PACK32, // HDR
        .image_count = REN_MAX_FRAMES_IN_FLIGHT + 1,
        .present_mode = rhi::Present_Mode::Immediate
        }))
    , m_gpu_transfer_context(m_device.get())
    , m_frames()
    , m_frame_counter(0)
    , m_asset_repository(std::make_unique<Asset_Repository>(
        m_logger,
        m_device.get(),
        Asset_Repository_Paths {
        .shaders = "../assets/shaders/",
        .pipelines = "../assets/pipelines/",
        .shader_include_paths = {
            "../",
            "../../src/shared/"
        },
        .models = "../assets/cache/"},
        *this))
    , m_resource_blackboard(std::make_unique<Render_Resource_Blackboard>(m_device.get()))
    , m_static_scene_data(std::make_unique<Static_Scene_Data>(
        m_device.get(),
        m_logger,
        m_gpu_transfer_context,
        *m_asset_repository,
        *m_resource_blackboard))
    , m_renderer(
        m_gpu_transfer_context,
        *m_swapchain,
        *m_asset_repository,
        *m_resource_blackboard)
    , m_is_running(true)
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

        auto& frame = m_frames[m_frame_counter % REN_MAX_FRAMES_IN_FLIGHT];

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

    if (m_input_state->is_key_clicked(SDL_SCANCODE_F5))
    {
        m_logger->info("Recompiling shaders and recreating pipelines.");
        m_device->wait_idle();
        m_asset_repository->recompile_shaders();
    }

    m_gpu_transfer_context.garbage_collect();

    m_renderer.setup_frame();
}

void Application::render_frame(Frame& frame, double t, double dt) noexcept
{
    auto graphics_cmd = frame.graphics_command_pool->acquire_command_list();
    auto upload_cmd = frame.graphics_command_pool->acquire_command_list();

    m_renderer.render(*m_static_scene_data, graphics_cmd, t, dt);
    m_gpu_transfer_context.process_immediate_uploads_on_graphics_queue(upload_cmd);

    auto cmds = std::to_array({ upload_cmd, graphics_cmd });

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

void Application::process_gui() noexcept
{
    ImGui::NewFrame();

    imgui_menubar();

    if (m_imgui_data.windows.renderer_settings)
    {
        imutil::push_minimum_window_size();
        if (ImGui::Begin("Renderer Settings", &m_imgui_data.windows.renderer_settings))
        {
            m_renderer.process_gui();
        }
        ImGui::End();
    }
    if (m_imgui_data.windows.demo)
        ImGui::ShowDemoWindow(&m_imgui_data.windows.demo);

    imgui_process_modals();

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
}

void Application::imgui_process_modals() noexcept
{
    constexpr static auto MODAL_FLAGS = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    constexpr static auto MODAL_WIDTH = 1280.f;
    constexpr static auto MODAL_HEIGHT = 720.f;
    if (m_imgui_data.modals.add_model)
    {
        constexpr static auto SCENE_SELECT_NAME = "Add Model";
        ImGui::OpenPopup(SCENE_SELECT_NAME);
        ImGui::SetNextWindowSize({MODAL_WIDTH, MODAL_HEIGHT}, ImGuiCond_Always);
        ImGui::SetNextWindowPos({
            (m_window->get_window_data().width - MODAL_WIDTH) / 2.f,
            (m_window->get_window_data().height - MODAL_HEIGHT) / 2.f});
        if (ImGui::BeginPopupModal(SCENE_SELECT_NAME, &m_imgui_data.modals.add_model, MODAL_FLAGS))
        {
            const auto model_files =  m_asset_repository->get_model_files();
            static std::string selected = "";
            if (ImGui::BeginListBox("##Models", ImVec2(MODAL_WIDTH - 20.f, MODAL_HEIGHT -90.f)))
            {
                for (auto& file : model_files)
                {
                    auto is_selected = file == selected;
                    if (ImGui::Selectable(file.c_str(), is_selected))
                    {
                        selected = file;
                    }
                }
                ImGui::EndListBox();
            }
            if (ImGui::Button("Add"))
            {
                m_imgui_data.modals.add_model = false;
                const Model_Descriptor descriptor = {
                    .name = selected,
                    .instances = {
                        {
                            .translation = { 0.f, 0.f, 0.f },
                            .rotation = glm::identity<glm::quat>(),
                            .scale = { 1.f, 1.f, 1.f }
                        }
                    }
                };
                if (!selected.empty())
                    m_static_scene_data->add_model(descriptor);
            }
            ImGui::EndPopup();
        }
    }
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
            if (ImGui::MenuItem("Add Model..."))
            {
                m_imgui_data.modals.add_model = true;
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
        if (ImGui::BeginMenu("Debug"))
        {
            imgui_menu_toggle_window("ImGui Demo Window", m_imgui_data.windows.demo);
            ImGui::EndMenu();
        }
        const float required_width = ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - required_width);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::Button("Close"))
        {
            m_is_running = false;
        }
        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}
}
