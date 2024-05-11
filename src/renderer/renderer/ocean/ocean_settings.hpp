#pragma once

#include "renderer/imgui/renderer_settings.hpp"

namespace ren
{
class Ocean_Settings : public Settings_Base
{
public:
    Ocean_Settings();
    virtual ~Ocean_Settings() noexcept = default;

    virtual void process_gui() override;

private:

};
}
