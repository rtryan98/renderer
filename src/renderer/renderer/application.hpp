#pragma once

#include "renderer/logger.hpp"
#include "renderer/window.hpp"
#include "renderer/renderer.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/render_resource_blackboard.hpp"
#include "renderer/gpu_transfer.hpp"

#include <rhi/graphics_device.hpp>
#include <rhi/swapchain.hpp>

#include "scene/scene.hpp"

namespace ren
{
struct ImGui_Data
{
    struct
    {
        bool demo = false;
        bool renderer_settings = false;
    } windows;
    struct
    {
        bool add_model = false;
    } modals;
};

struct Application_Create_Info
{
    uint32_t width;
    uint32_t height;
    bool enable_validation;
    bool enable_gpu_validation;
};

class Application
{
public:
    Application(const Application_Create_Info& create_info) noexcept;
    ~Application() noexcept;

    void run();

private:
    struct Frame
    {
        rhi::Fence* frame_fence = nullptr;
        uint64_t fence_value = 0;
        std::unique_ptr<rhi::Command_Pool> graphics_command_pool;
        std::unique_ptr<rhi::Command_Pool> compute_command_pool;
        std::unique_ptr<rhi::Command_Pool> copy_command_pool;
    };

    void setup_frame(Frame& frame) noexcept;
    void render_frame(Frame& frame, double t, double dt) noexcept;
    void process_gui() noexcept;
    void update(double t, double dt) noexcept;

    void imgui_close_all_windows() noexcept;
    void imgui_process_modals() noexcept;
    void imgui_menubar() noexcept;

private:
    std::shared_ptr<Logger> m_logger;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Input_State> m_input_state;
    std::unique_ptr<rhi::Graphics_Device> m_device;
    std::unique_ptr<rhi::Swapchain> m_swapchain;
    GPU_Transfer_Context m_gpu_transfer_context;
    std::array<Frame, REN_MAX_FRAMES_IN_FLIGHT> m_frames;
    uint64_t m_frame_counter;
    std::unique_ptr<Asset_Repository> m_asset_repository;
    std::unique_ptr<Render_Resource_Blackboard> m_resource_blackboard;
    std::unique_ptr<Static_Scene_Data> m_static_scene_data;
    int32_t m_display_peak_luminance = 250;
    bool m_enable_hdr = false;
    bool m_is_hdr_mode_changed = false;
    bool m_is_hdr_luminance_changed = false;
    Renderer m_renderer;
    bool m_is_running;
    ImGui_Data m_imgui_data = {};
};
}
