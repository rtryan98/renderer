#pragma once

#include "renderer/logger.hpp"
#include "renderer/window.hpp"
#include "renderer/asset_manager.hpp"
#include "renderer/cbt/cbt_cpu.hpp"
#include "renderer/shader_manager.hpp"
#include "renderer/imgui/renderer_settings.hpp"
#include "renderer/ocean/ocean_renderer.hpp"
#include "renderer/renderer.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <rhi/graphics_device.hpp>
#include <rhi/swapchain.hpp>
#include <rhi/resource.hpp>

namespace ren
{
constexpr static std::size_t FRAME_IN_FLIGHT_COUNT = 2;

struct ImGui_Data
{
    struct
    {
        bool demo = false;
        bool renderer_settings = false;
        bool tool_cbt_vis = false;
    } windows;
};

class Application
{
public:
    Application() noexcept;
    ~Application() noexcept;

    void run();
    Window& get_window() const { return *m_window; };

    void upload_buffer_data_immediate(rhi::Buffer* buffer, void* data, uint64_t size, uint64_t offset) noexcept;


private:
    struct Frame
    {
        rhi::Fence* frame_fence = nullptr;
        uint64_t fence_value = 0;
        std::unique_ptr<rhi::Command_Pool> graphics_command_pool;
        std::unique_ptr<rhi::Command_Pool> compute_command_pool;
        std::unique_ptr<rhi::Command_Pool> copy_command_pool;
    };

    struct Buffer_Staging_Info
    {
        rhi::Buffer* src;
        rhi::Buffer* dst;
        uint64_t offset;
        uint64_t size;
    };

    void setup_frame(Frame& frame) noexcept;
    void render_frame(Frame& frame, double t, double dt) noexcept;
    rhi::Command_List* handle_immediate_uploads(Frame& frame) noexcept;
    void process_gui() noexcept;
    void update(double t, double dt) noexcept;

    void imgui_close_all_windows() noexcept;

    void imgui_setup_style() noexcept;
    void imgui_menubar() noexcept;

private:
    std::shared_ptr<Logger> m_logger;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Input_State> m_input_state;
    std::unique_ptr<rhi::Graphics_Device> m_device;
    std::unique_ptr<rhi::Swapchain> m_swapchain;
    std::unique_ptr<Asset_Repository> m_asset_repository;
    Shader_Library_Legacy m_shader_library;
    Asset_Manager m_asset_manager;
    Renderer m_renderer;
    std::array<Frame, FRAME_IN_FLIGHT_COUNT> m_frames;
    std::array<std::vector<rhi::Buffer*>, FRAME_IN_FLIGHT_COUNT> m_staging_buffers;
    std::array<std::vector<Buffer_Staging_Info>, FRAME_IN_FLIGHT_COUNT> m_buffer_staging_infos;
    uint64_t m_frame_counter;
    bool m_is_running;
    ImGui_Data m_imgui_data = {};
    std::unique_ptr<CBT_CPU_Vis> m_cbt_cpu_vis;
    Renderer_Settings m_renderer_settings;
};
}
