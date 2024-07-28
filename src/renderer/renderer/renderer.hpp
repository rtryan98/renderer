#pragma once

namespace ren
{
class Application;
class Asset_Manager;
struct Render_Attachment;

class Renderer
{
public:
    Renderer(Application& app, Asset_Manager& asset_manager);

private:
    void init_g_buffer();

private:
    Application& m_app;
    Asset_Manager& m_asset_manager;

    float m_render_scale = 1.f;
    struct
    {
        Render_Attachment* target0;
        Render_Attachment* depth_stencil;
    } m_g_buffer = {};
};
}
