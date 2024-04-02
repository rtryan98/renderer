#pragma once

#include "window.hpp"
#include "renderer/imgui_renderer.hpp"

#include <rhi/graphics_device.hpp>
#include <rhi/swapchain.hpp>
#include <rhi/resource.hpp>

namespace ren
{
constexpr static std::size_t FRAME_IN_FLIGHT_COUNT = 2;

class Application
{
public:
    Application() noexcept;
    ~Application() noexcept;

    void run();

private:
    struct Frame
    {
        rhi::Fence* frame_fence;
        uint64_t fence_value;
        std::unique_ptr<rhi::Command_Pool> graphics_command_pool;
        std::unique_ptr<rhi::Command_Pool> compute_command_pool;
        std::unique_ptr<rhi::Command_Pool> copy_command_pool;
    };

    void setup_frame(Frame& frame) noexcept;
    void render_frame(Frame& frame, double t, double dt) noexcept;

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<rhi::Graphics_Device> m_device;
    std::unique_ptr<rhi::Swapchain> m_swapchain;
    std::array<Frame, FRAME_IN_FLIGHT_COUNT> m_frames;
    uint64_t m_frame_counter;
    bool m_is_running;
    std::unique_ptr<Imgui_Renderer> m_imgui_renderer;
};
}
