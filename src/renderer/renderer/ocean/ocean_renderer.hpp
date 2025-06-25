#pragma once

#include "renderer/ocean/ocean_settings.hpp"
#include "renderer/ocean/ocean_resources.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Manager;
class Shader_Library_Legacy;
class Application;

class Ocean_Renderer
{
public:
    Ocean_Renderer(Asset_Manager& asset_manager, Shader_Library_Legacy& shader_library);
    ~Ocean_Renderer();

    [[nodiscard]] Ocean_Settings* get_settings() noexcept;

    void simulate(Application& app, rhi::Command_List* cmd, float dt) noexcept;

    void render(rhi::Command_List* cmd) noexcept;
    void render_patch(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;
    void render_composite(rhi::Command_List* cmd, uint32_t color_image_bindless_index) noexcept;

    void debug_render_slope(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;
    void debug_render_normal(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;

private:
    Asset_Manager& m_asset_manager;
    Shader_Library_Legacy& m_shader_library;
    Ocean_Resources m_resources;
    Ocean_Settings m_settings;
};
}
