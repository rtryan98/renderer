#pragma once

#include "renderer/imgui/imgui_renderer.hpp"
#include "renderer/ocean/ocean_renderer.hpp"
#include "renderer/scene/camera.hpp"
#include "renderer/render_resource_blackboard.hpp"

namespace rhi
{
class Command_List;
class Swapchain;
}

namespace ren
{
class Application;
class Input_State;
struct Render_Attachment;
class Render_Resource_Blackboard;

class Renderer
{
public:
    Renderer(Application& app, rhi::Swapchain& swapchain,
        Render_Resource_Blackboard& resource_blackboard,
        const Imgui_Renderer_Create_Info& imgui_renderer_create_info);

    std::vector<Settings_Base*> get_settings() noexcept;

    void update(const Input_State& input_state, double t, double dt) noexcept;
    void overlay_gui();
    void setup_frame();
    void render(rhi::Command_List* cmd, double t, double dt) noexcept;
    void on_resize(uint32_t width, uint32_t height) noexcept;

private:
    void render_gbuffer_pass(rhi::Command_List* cmd);
    void render_ocean_pass(rhi::Command_List* cmd);
    void render_swapchain_pass(rhi::Command_List* cmd);

    void init_rendertargets();

private:
    Application& m_app;
    rhi::Swapchain& m_swapchain;
    Render_Resource_Blackboard& m_resource_blackboard;

    Fly_Camera m_fly_cam;
    Buffer m_camera_buffer;
    Imgui_Renderer m_imgui_renderer;
    Ocean_Renderer m_ocean_renderer;

    float m_render_scale = 1.f;
    struct
    {
        Image target0;
        Image depth_stencil;
    } m_g_buffer = {};
    struct
    {
        Image color;
        Image depth_stencil;
    } m_ocean_rendertargets;

    bool m_should_display_overlay;
};
}
