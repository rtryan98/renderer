#pragma once

#include "renderer/imgui/imgui_renderer.hpp"
#include "renderer/ocean/ocean_renderer.hpp"
#include "renderer/scene/camera.hpp"

namespace rhi
{
class Command_List;
class Swapchain;
}

namespace ren
{
class Application;
class Asset_Manager;
class Shader_Library;
class Input_State;
struct Render_Attachment;

class Renderer
{
public:
    Renderer(Application& app, Asset_Manager& asset_manager,
        Shader_Library& shader_library, rhi::Swapchain& swapchain,
        const Imgui_Renderer_Create_Info& imgui_renderer_create_info);

    std::vector<Settings_Base*> get_settings() noexcept;

    void update(const Input_State& input_state, double t, double dt) noexcept;
    void overlay_gui();
    void setup_frame();
    void render(rhi::Command_List* cmd, double t, double dt) noexcept;

private:
    void render_gbuffer_pass(rhi::Command_List* cmd);
    void render_ocean_pass(rhi::Command_List* cmd);
    void render_swapchain_pass(rhi::Command_List* cmd);

    void init_rendertargets();

private:
    Application& m_app;
    Asset_Manager& m_asset_manager;
    Shader_Library& m_shader_library;
    rhi::Swapchain& m_swapchain;

    Fly_Camera m_fly_cam;
    rhi::Buffer* m_camera_buffer;
    Imgui_Renderer m_imgui_renderer;
    Ocean_Renderer m_ocean_renderer;

    float m_render_scale = 1.f;
    struct
    {
        Render_Attachment* target0;
        Render_Attachment* depth_stencil;
    } m_g_buffer = {};
    struct
    {
        Render_Attachment* color;
        Render_Attachment* depth_stencil;
    } m_ocean_rendertargets;

    bool m_should_display_overlay;
};
}
