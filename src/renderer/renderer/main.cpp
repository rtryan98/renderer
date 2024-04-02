#include <cstdint>
#include <imgui.h>

#include "renderer/application.hpp"

int32_t main([[maybe_unused]] uint32_t argc, [[maybe_unused]] const char* argv[])
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto application = std::make_unique<ren::Application>();
    application->run();
    application = nullptr; // explicit delete for imgui

    ImGui::DestroyContext();
    return 0;
}
