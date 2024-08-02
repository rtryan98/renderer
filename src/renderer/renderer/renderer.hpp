#pragma once

#include "renderer/imgui/imgui_renderer.hpp"

namespace rhi
{
class Command_List;
class Swapchain;
}

namespace ren
{
class Application;
class Asset_Manager;
struct Render_Attachment;

class Renderer
{
public:
    Renderer(Application& app, Asset_Manager& asset_manager,
        rhi::Swapchain& swapchain,
        const Imgui_Renderer_Create_Info& imgui_renderer_create_info);

    void update(double t, double dt) noexcept;
    void setup_frame();
    void render(rhi::Command_List* cmd) noexcept;

private:
    void render_swapchain_pass(rhi::Command_List* cmd);

    void init_g_buffer();

private:
    Application& m_app;
    Asset_Manager& m_asset_manager;
    rhi::Swapchain& m_swapchain;

    Imgui_Renderer m_imgui_renderer;

    float m_render_scale = 1.f;
    struct
    {
        Render_Attachment* target0;
        Render_Attachment* depth_stencil;
    } m_g_buffer = {};
};
}
