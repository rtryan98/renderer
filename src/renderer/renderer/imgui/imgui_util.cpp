#include "renderer/imgui/imgui_util.hpp"

#include <imgui.h>

namespace ren::imutil
{
void help_marker(const char* text, bool is_same_line)
{
    if (is_same_line) ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
}
