#include "renderer/ocean/ocean_renderer.hpp"

namespace ren
{
Ocean_Renderer::Ocean_Renderer()
    : m_resources()
    , m_settings(m_resources)
{}

Ocean_Settings* Ocean_Renderer::get_settings() noexcept
{
    return &m_settings;
}
}
