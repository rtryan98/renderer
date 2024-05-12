#pragma once

#include "renderer/imgui/renderer_settings.hpp"

namespace ren
{
struct Ocean_Resources;
class Asset_Manager;
class Shader_Library;

class Ocean_Settings : public Settings_Base
{
public:
    Ocean_Settings(Ocean_Resources& resources, Asset_Manager& asset_manager, Shader_Library& shader_library);
    virtual ~Ocean_Settings() noexcept = default;

    virtual void process_gui() override;

private:
    void process_gui_options();
    void process_gui_simulation_settings();

private:
    Ocean_Resources& m_resources;
    Asset_Manager& m_asset_manager;
    Shader_Library& m_shader_library;
};
}
