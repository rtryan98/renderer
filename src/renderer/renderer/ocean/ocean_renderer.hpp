#pragma once

#include "renderer/ocean/ocean_settings.hpp"
#include "renderer/ocean/ocean_resources.hpp"

namespace ren
{
class Ocean_Renderer
{
public:
    Ocean_Renderer();

    [[nodiscard]] Ocean_Settings* get_settings() noexcept;

private:
    Ocean_Resources m_resources;
    Ocean_Settings m_settings;
};
}
