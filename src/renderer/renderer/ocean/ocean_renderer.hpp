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
class Shader_Library;
class Application;

class Ocean_Renderer
{
public:
    Ocean_Renderer(Asset_Manager& asset_manager, Shader_Library& shader_library);
    ~Ocean_Renderer();

    [[nodiscard]] Ocean_Settings* get_settings() noexcept;

    void simulate(Application& app, rhi::Command_List* cmd, float dt) noexcept;

private:
    Asset_Manager& m_asset_manager;
    Shader_Library& m_shader_library;
    Ocean_Resources m_resources;
    Ocean_Settings m_settings;
};
}
