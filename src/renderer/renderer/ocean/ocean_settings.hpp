#pragma once

#include "renderer/imgui/renderer_settings.hpp"

namespace ren
{
struct Ocean_Resources;

class Ocean_Settings : public Settings_Base
{
public:
    Ocean_Settings(Ocean_Resources& resources);
    virtual ~Ocean_Settings() noexcept = default;

    virtual void process_gui() override;

private:
    Ocean_Resources& m_resources;
};
}
