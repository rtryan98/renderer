#include "renderer/imgui/imgui_util.hpp"

#include <imgui.h>

namespace ren::imutil
{
static float dpi_scale = 1.0f;

void help_marker(const char* text, bool is_same_line)
{
    if (is_same_line) ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f * get_dpi_scale());
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void set_dpi_scale(float scale)
{
    dpi_scale = scale;
}

float get_dpi_scale()
{
    return dpi_scale;
}

Context_Wrapper::Context_Wrapper() noexcept
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
}

Context_Wrapper::~Context_Wrapper() noexcept
{
    ImGui::DestroyContext();
}
}
