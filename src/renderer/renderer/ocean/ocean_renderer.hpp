#pragma once

#include "renderer/ocean/ocean_settings.hpp"
#include "renderer/ocean/ocean_resources.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Application;
class Asset_Repository;
class Render_Resource_Blackboard;

class Ocean_Renderer
{
public:
    Ocean_Renderer(Render_Resource_Blackboard& resource_blackboard, Asset_Repository& asset_repository);
    ~Ocean_Renderer();

    [[nodiscard]] Ocean_Settings* get_settings() noexcept;

    void simulate(Application& app, rhi::Command_List* cmd, float dt) noexcept;

    void render(rhi::Command_List* cmd) noexcept;
    void render_patch(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;
    void render_composite(rhi::Command_List* cmd,
        uint32_t ocean_color_idx,
        uint32_t ocean_depth_idx,
        uint32_t geom_color_idx,
        uint32_t geom_depth_idx) noexcept;

    void debug_render_slope(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;
    void debug_render_normal(rhi::Command_List* cmd, uint32_t camera_buffer_bindless_index) noexcept;

private:
    Render_Resource_Blackboard& m_resource_blackboard;
    Asset_Repository& m_asset_repository;
    Ocean_Resources m_resources;
    Ocean_Settings m_settings;
};
}
