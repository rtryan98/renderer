#include "renderer/imgui/renderer_settings.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ren
{
Settings_Base::Settings_Base(const std::string_view name) noexcept
    : m_name(name)
{}

const std::string& Settings_Base::get_name() noexcept
{
    return m_name;
}

constexpr static const char* RENDERER_SETTINGS_NAME = "Renderer Settings";

void Renderer_Settings::process_gui(bool* active)
{
    static std::size_t last_size = 0;

    if (last_size != m_settings.size())
    {
        for (auto setting : m_settings)
        {
            m_listbox_width = std::max(ImGui::CalcTextSize(setting->get_name().c_str(), nullptr, true).x + ImGui::GetFontSize(), m_listbox_width);
        }
        last_size = m_settings.size();
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(500.f, 350.f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin(RENDERER_SETTINGS_NAME, active, ImGuiWindowFlags_NoCollapse))
    {
        auto avail = ImGui::GetContentRegionAvail();
        if (ImGui::BeginChild("Left##Renderer Settings", ImVec2(m_listbox_width, avail.y)))
        {
            if (ImGui::BeginListBox(
                "##Renderer Settings Listbox",
                ImVec2(-FLT_MIN, avail.y)))
            {
                for (uint32_t i = 0; i < m_settings.size(); ++i)
                {
                    bool is_selected = i == m_selected;
                    if (ImGui::Selectable(m_settings[i]->get_name().c_str(), is_selected))
                    {
                        m_selected = i;
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("Right##Renderer Settings", ImVec2(ImGui::GetContentRegionAvail().x, avail.y), true))
        {
            if (m_selected < m_settings.size() && m_settings[m_selected])
            {
                m_settings[m_selected]->process_gui();
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

}

void Renderer_Settings::add_settings(Settings_Base* settings) noexcept
{
    m_settings.push_back(settings);
}
}
