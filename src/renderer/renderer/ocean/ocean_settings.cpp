#include "renderer/ocean/ocean_settings.hpp"

#include <imgui.h>

namespace ren
{
Ocean_Settings::Ocean_Settings()
    : Settings_Base("Ocean")
{

}

void Ocean_Settings::process_gui()
{
    ImGui::SeparatorText("Simulation Settings");
    static float dir = 0.f;
    ImGui::PushItemWidth(-150.f);
    ImGui::SliderFloat("Direction", &dir, 0.f, 359.995f, "%.3f deg", ImGuiSliderFlags_AlwaysClamp);
}
}
